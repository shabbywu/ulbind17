#include "../samples/common.hpp"
#include <ulbind17/runtime.hpp>

using sample::check;
struct PaddedSurface final : ultralight::Surface {
    std::uint32_t w = 0, h = 0, stride = 0;
    std::vector<std::byte> bytes;
    bool locked = false;
    int unlocks = 0;
    PaddedSurface(std::uint32_t width, std::uint32_t height) { Resize(width, height); }
    std::uint32_t width() const override { return w; }
    std::uint32_t height() const override { return h; }
    std::uint32_t row_bytes() const override { return stride; }
    std::size_t size() const override { return bytes.size(); }
    void *LockPixels() override {
        check(!locked, "Surface double-locked"); locked = true; return bytes.data();
    }
    void UnlockPixels() override { locked = false; ++unlocks; }
    void Resize(std::uint32_t width, std::uint32_t height) override {
        check(!locked, "Surface resized while locked");
        w = width; h = height; stride = ((w * 4 + 15) / 16 + 1) * 16;
        bytes.assign(std::size_t(stride) * h, std::byte{0});
    }
    bool Scroll(const ultralight::IntRect &rect, int dx, int dy) override {
        check(locked, "Scroll called without pixel lock");
        ShiftPixels(bytes.data(), stride, rect, dx, dy); return false;
    }
};
struct Factory final : ultralight::SurfaceFactory {
    int created = 0, destroyed = 0;
    ultralight::Surface *CreateSurface(std::uint32_t w, std::uint32_t h) override {
        ++created; return new PaddedSurface(w, h);
    }
    void DestroySurface(ultralight::Surface *surface) override { ++destroyed; delete surface; }
};
// A non-rendering GPU driver is enough to create a GPU View and check that the
// CPU API rejects it. This is not a GPU rendering test.
struct NullGPU final : ultralight::GPUDriver {
    std::uint32_t next = 1;
    void BeginSynchronize() override {}
    void EndSynchronize() override {}
    std::uint32_t NextTextureId() override { return next++; }
    void CreateTexture(std::uint32_t, ultralight::RefPtr<ultralight::Bitmap>, std::uint32_t) override {}
    void UpdateTexture(std::uint32_t, ultralight::RefPtr<ultralight::Bitmap>, const ultralight::IntRect &) override {}
    void DestroyTexture(std::uint32_t) override {}
    std::uint32_t NextRenderBufferId() override { return next++; }
    void CreateRenderBuffer(std::uint32_t, const ultralight::RenderBuffer &) override {}
    void DestroyRenderBuffer(std::uint32_t) override {}
    std::uint32_t NextGeometryId() override { return next++; }
    void CreateGeometry(std::uint32_t, const ultralight::VertexBuffer &, const ultralight::IndexBuffer &) override {}
    void UpdateGeometry(std::uint32_t, const ultralight::VertexBuffer &, const ultralight::IndexBuffer &) override {}
    void DestroyGeometry(std::uint32_t) override {}
    void UpdateCommandList(const ultralight::CommandList &) override {}
};
template <typename F> void busy(F callback) {
    try { callback(); } catch (const ulbind17::Error &error) {
        check(error.code == ulbind17::ErrorCode::Busy, error.what()); return;
    }
    throw std::runtime_error("Expected locked-pixel error");
}
int main() {
    // Static services outlive every Renderer reference.
    static Factory factory;
    static NullGPU gpu;
    return sample::run([&] {
        auto &platform = ultralight::Platform::instance();
        platform.set_surface_factory(&factory); platform.set_gpu_driver(&gpu);
        auto runtime = ulbind17::Runtime::Create();
        ultralight::ViewConfig config; config.is_accelerated = false; config.is_transparent = true;
        auto view = runtime.CreateView(31, 23, config);
        bool ready = false;
        ulbind17::ViewCallbacks callbacks;
        callbacks.dom_ready = [&](const auto &event) { if (event.is_main_frame) ready = true; };
        view.SetCallbacks(callbacks);
        view.LoadHTML("<style>html,body{margin:0;height:100%;background:rgba(255,0,0,.5)}</style><body></body>");
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!ready && std::chrono::steady_clock::now() < deadline) runtime.Update();
        check(ready, "Page did not load"); runtime.RefreshDisplay(); runtime.Render();
        auto *surface = static_cast<PaddedSurface *>(view.native()->surface());
        auto unlocks = surface->unlocks;
        try {
            view.WithPixels([&](const ulbind17::PixelView &pixels) {
                check(pixels.width == 31 && pixels.height == 23 && pixels.row_bytes > 31 * 4,
                      "Custom stride was discarded");
                check(pixels.data.size() == pixels.row_bytes * pixels.height, "Invalid pixel span");
                busy([&] { runtime.Update(); }); busy([&] { runtime.Render(); });
                busy([&] { view.Resize(2, 2); }); busy([&] { view.Close(); }); busy([&] { runtime.Close(); });
                busy([&] { view.LoadHTML("replaced"); }); busy([&] { view.CopyPixels(); });
                throw std::runtime_error("pixel callback failure");
            });
        } catch (const std::runtime_error &error) {
            check(std::string(error.what()) == "pixel callback failure", error.what());
        }
        check(!surface->locked && surface->unlocks == unlocks + 1, "Exception did not unlock exactly once");
        auto pixels = view.CopyPixels();
        check(!pixels.dirty_bounds.IsEmpty() && !surface->dirty_bounds().IsEmpty(), "Read cleared dirty bounds");
        auto channel = [&](int index) { return std::to_integer<int>(pixels.data[index]); };
        check(channel(0) == 0 && channel(1) == 0 && channel(2) >= 120 && channel(2) <= 200 &&
              channel(2) == channel(3), "Pixels are not premultiplied BGRA");
        view.ClearDirtyBounds(); check(surface->dirty_bounds().IsEmpty(), "Dirty acknowledgement failed");
        view.Resize(19, 11); runtime.RefreshDisplay(); runtime.Render();
        auto resized = view.CopyPixels();
        check(resized.width == 19 && resized.height == 11 && !resized.dirty_bounds.IsEmpty(), "Resize frame invalid");
        view.Close(); check(pixels.width == 31 && !pixels.data.empty(), "Copied frame was tied to View lifetime");
        config.is_accelerated = true;
        auto accelerated = runtime.CreateView(10, 10, config);
        try { accelerated.CopyPixels(); throw std::logic_error("GPU View accepted CPU access"); }
        catch (const ulbind17::Error &error) { check(error.code == ulbind17::ErrorCode::Unsupported, error.what()); }
        runtime.Close(); check(factory.created == factory.destroyed, "Surface was not destroyed");
    });
}
