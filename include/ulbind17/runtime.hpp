#pragma once
#include <Ultralight/Ultralight.h>
#include <Ultralight/js/API.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <ulbind17/Error.hpp>
#include <ulbind17/jsc/Bridge.hpp>
#include <utility>
#include <vector>

namespace ulbind17 {
class Runtime;
class ManagedView;
class API;
namespace appcore_detail {
struct Access;
}

struct FrameEvent {
    std::uint64_t frame_id;
    bool is_main_frame;
    std::string url;
};
struct LoadFailure {
    FrameEvent frame;
    std::string description, domain;
    int code;
};
struct ViewCallbacks {
    std::function<void(const FrameEvent &)> begin_loading, window_ready, dom_ready, finish_loading;
    std::function<void(const LoadFailure &)> fail_loading;
    std::function<void()> history_changed;
    std::function<void(std::string_view)> page_settled, title_changed, url_changed, tooltip_changed;
    std::function<void(ultralight::Cursor)> cursor_changed;
    // ConsoleMessage is borrowed for the duration of the callback.
    std::function<void(const ultralight::ConsoleMessage &)> console;
    std::function<void()> request_close;
};

enum class PixelFormat {
    BGRA8Premultiplied
};
struct PixelView {
    std::uint32_t width, height, row_bytes;
    PixelFormat format = PixelFormat::BGRA8Premultiplied;
    ultralight::IntRect dirty_bounds;
    std::span<const std::byte> data;
};
struct PixelBuffer {
    std::uint32_t width = 0, height = 0, row_bytes = 0;
    PixelFormat format = PixelFormat::BGRA8Premultiplied;
    ultralight::IntRect dirty_bounds;
    std::vector<std::byte> data;
};

namespace runtime_detail {
struct ViewState;
// Optional hosts implement these interfaces without adding SDK/window dependencies here.
struct ExternalHost {
    virtual ~ExternalHost() = default;
    virtual bool IsClosed() const noexcept = 0;
    virtual void Close() noexcept = 0;
};
struct ViewHost {
    virtual ~ViewHost() = default;
    virtual void Focus() = 0;
    virtual void Unfocus() = 0;
    virtual void Close() noexcept = 0;
};
struct RuntimeState {
    const std::thread::id thread = std::this_thread::get_id();
    std::mutex tasks_mutex;
    ultralight::RefPtr<ultralight::Renderer> renderer;
    std::atomic<bool> closed = false;
    bool busy = false;
    std::shared_ptr<ExternalHost> external_host;
    // SDK ownership retained across calls which may close the external host in a callback.
    std::shared_ptr<void> host_keepalive;
    std::size_t pixel_locks = 0;
    std::size_t bridge_scopes = 0;
    std::vector<std::weak_ptr<ViewState>> views;
    std::function<void(const Error &)> error_handler;

    explicit RuntimeState(ultralight::RefPtr<ultralight::Renderer> renderer) : renderer(std::move(renderer)) {
    }
    ~RuntimeState() {
        // An external host may own the GPU driver used by this Renderer.
        renderer = nullptr;
        host_keepalive.reset();
        external_host.reset();
    }
    void require_thread() const {
        if (std::this_thread::get_id() != thread)
            throw Error(ErrorCode::WrongThread, "Operation requires the renderer thread");
    }
    void require_open() const {
        require_thread();
        if (closed)
            throw Error(ErrorCode::Closed, "Runtime is closed");
    }
    void require_unlocked() const {
        if (pixel_locks)
            throw Error(ErrorCode::Busy, "Operation is unavailable while pixels are locked");
    }
    void report(const Error &error) noexcept {
        try {
            auto handler = error_handler;
            if (handler) {
                handler(error);
                return;
            }
        }
        catch (...) {
        }
        try {
            auto logger = ultralight::Platform::instance().logger();
            if (logger)
                logger->LogMessage(ultralight::LogLevel::Error, error.what());
        }
        catch (...) {
        }
    }
};

struct ViewState final : ultralight::LoadListener, ultralight::ViewListener, std::enable_shared_from_this<ViewState> {
    std::shared_ptr<RuntimeState> runtime;
    ultralight::RefPtr<ultralight::View> view;
    std::shared_ptr<jsc::Bridge> bridge;
    ViewCallbacks callbacks;
    // Borrowed SDK refs do not retain an API owning handle.
    std::vector<ultralight::js::API> attached_apis;
    std::shared_ptr<ViewHost> host;
    bool closed = false;
    std::size_t pixel_locks = 0;

    ViewState(std::shared_ptr<RuntimeState> runtime, ultralight::RefPtr<ultralight::View> view)
        : runtime(std::move(runtime)), view(std::move(view)) {
    }
    ~ViewState() {
        close();
    } // Final release must be on the renderer thread.
    void require_open() const {
        runtime->require_thread();
        if (closed || runtime->closed)
            throw Error(ErrorCode::Closed, "View is closed");
    }
    void require_unlocked() const {
        if (pixel_locks)
            throw Error(ErrorCode::Busy, "View pixels are locked");
    }
    void expire() {
        auto old = std::exchange(bridge, {});
        if (old)
            old->invalidate();
    }
    void close() noexcept {
        if (closed)
            return;
        closed = true;
        // Keep resources alive through cancellation, which may call Close again.
        auto native = std::exchange(view, {});
        if (native.get()) {
            native->set_load_listener(nullptr);
            native->set_view_listener(nullptr);
            auto apis = std::exchange(attached_apis, {});
            for (auto &api : apis)
                api.DetachFrom(native.get());
        }
        expire();
        callbacks = {};
        auto hosting = std::exchange(host, {});
        if (hosting)
            hosting->Close();
    }
    template <typename F> void dispatch(F select) noexcept {
        auto keep_alive = shared_from_this();
        auto native = view;
        if (closed || runtime->closed)
            return;
        try {
            select();
        }
        catch (const Error &error) {
            runtime->report(error);
        }
        catch (const std::exception &error) {
            runtime->report(Error(ErrorCode::HostException, error.what()));
        }
        catch (...) {
            runtime->report(Error(ErrorCode::HostException, "Unknown host callback exception"));
        }
    }
    template <typename Member, typename... Args> void emit(Member member, Args &&...args) {
        auto callback = callbacks.*member;
        if (callback)
            callback(std::forward<Args>(args)...);
    }
    static std::string utf8(const ultralight::String &text) {
        auto value = text.utf8();
        return std::string(value.data(), value.length());
    }
    FrameEvent frame(std::uint64_t id, bool main, const ultralight::String &url) {
        return {id, main, utf8(url)};
    }
    void OnBeginLoading(ultralight::View *, std::uint64_t id, bool main, const ultralight::String &url) override {
        dispatch([&] {
            auto event = frame(id, main, url);
            if (main)
                expire();
            if (!closed && !runtime->closed)
                emit(&ViewCallbacks::begin_loading, event);
        });
    }
    void OnWindowObjectReady(ultralight::View *, std::uint64_t id, bool main, const ultralight::String &url) override {
        dispatch([&] {
            auto event = frame(id, main, url);
            if (main)
                expire();
            if (!closed && !runtime->closed)
                emit(&ViewCallbacks::window_ready, event);
        });
    }
    void OnDOMReady(ultralight::View *, std::uint64_t id, bool main, const ultralight::String &url) override {
        dispatch([&] { emit(&ViewCallbacks::dom_ready, frame(id, main, url)); });
    }
    void OnFinishLoading(ultralight::View *, std::uint64_t id, bool main, const ultralight::String &url) override {
        dispatch([&] { emit(&ViewCallbacks::finish_loading, frame(id, main, url)); });
    }
    void OnFailLoading(ultralight::View *, std::uint64_t id, bool main, const ultralight::String &url,
                       const ultralight::String &description, const ultralight::String &domain, int code) override {
        dispatch([&] {
            emit(&ViewCallbacks::fail_loading,
                 LoadFailure{frame(id, main, url), utf8(description), utf8(domain), code});
        });
    }
    void OnUpdateHistory(ultralight::View *) override {
        dispatch([&] { emit(&ViewCallbacks::history_changed); });
    }
    void OnPageSettled(ultralight::View *, const ultralight::String &url) override {
        dispatch([&] { emit(&ViewCallbacks::page_settled, utf8(url)); });
    }
    void OnChangeTitle(ultralight::View *, const ultralight::String &text) override {
        dispatch([&] { emit(&ViewCallbacks::title_changed, utf8(text)); });
    }
    void OnChangeURL(ultralight::View *, const ultralight::String &text) override {
        dispatch([&] { emit(&ViewCallbacks::url_changed, utf8(text)); });
    }
    void OnChangeTooltip(ultralight::View *, const ultralight::String &text) override {
        dispatch([&] { emit(&ViewCallbacks::tooltip_changed, utf8(text)); });
    }
    void OnChangeCursor(ultralight::View *, ultralight::Cursor cursor) override {
        dispatch([&] { emit(&ViewCallbacks::cursor_changed, cursor); });
    }
    void OnAddConsoleMessage(ultralight::View *, const ultralight::ConsoleMessage &message) override {
        dispatch([&] { emit(&ViewCallbacks::console, message); });
    }
    void OnRequestClose(ultralight::View *) override {
        dispatch([&] { emit(&ViewCallbacks::request_close); });
    }
};
} // namespace runtime_detail

// Copyable handles share state. Dropping Runtime doesn't invalidate surviving Views;
// explicit Close does. Do not create a second Renderer in an existing host.
class Runtime {
    std::shared_ptr<runtime_detail::RuntimeState> state_;
    explicit Runtime(std::shared_ptr<runtime_detail::RuntimeState> state) : state_(std::move(state)) {
    }
    friend class ManagedView;
    friend struct appcore_detail::Access;
    struct Registry {
        std::mutex mutex;
        bool initialized = false;
        std::vector<std::weak_ptr<runtime_detail::RuntimeState>> runtimes;
    };
    static Registry &registry() {
        static Registry value;
        return value;
    }
    template <typename F> void drive(F callback) const {
        auto state = state_;
        state->require_open();
        state->require_unlocked();
        if (state->external_host)
            throw Error(ErrorCode::Unsupported, "This Renderer is driven by its external host");
        if (state->busy)
            throw Error(ErrorCode::Busy, "Renderer calls cannot be nested");
        state->busy = true;
        struct Guard {
            bool &busy;
            ~Guard() {
                busy = false;
            }
        } guard{state->busy};
        auto renderer = state->renderer;
        callback(*renderer.get());
    }

  public:
    static Runtime Create() {
        auto &platform = ultralight::Platform::instance();
        if (!platform.file_system() || !platform.font_loader())
            throw Error(ErrorCode::InvalidArgument, "Initialize FileSystem and FontLoader before creating a Runtime");
        auto &registered = registry();
        std::lock_guard lock(registered.mutex);
        for (const auto &weak : registered.runtimes)
            if (auto state = weak.lock(); state && !state->closed) {
                state->require_open();
                return Runtime(std::move(state));
            }
        if (registered.initialized)
            throw Error(ErrorCode::Closed, "Runtime already shut down; Renderer recreation is unsupported");
        auto state = std::make_shared<runtime_detail::RuntimeState>(ultralight::Renderer::Create());
        if (!state->renderer.get())
            throw Error(ErrorCode::InvalidArgument, "Renderer creation failed");
        registered.initialized = true;
        registered.runtimes.push_back(state);
        return Runtime(std::move(state));
    }
    // Call on the existing Renderer's thread. Never replaces platform handlers.
    static Runtime FromExisting(ultralight::RefPtr<ultralight::Renderer> renderer) {
        if (!renderer.get())
            throw Error(ErrorCode::InvalidArgument, "Empty Renderer");
        // Reuse shared state when another wrapper adopts this Renderer in this binary.
        auto &registered = registry();
        std::lock_guard lock(registered.mutex);
        std::erase_if(registered.runtimes, [](const auto &weak) { return weak.expired(); });
        for (const auto &weak : registered.runtimes)
            if (auto state = weak.lock()) {
                std::lock_guard tasks_lock(state->tasks_mutex);
                if (state->renderer.get() == renderer.get()) {
                    state->require_open();
                    return Runtime(std::move(state));
                }
            }
        auto state = std::make_shared<runtime_detail::RuntimeState>(std::move(renderer));
        registered.initialized = true;
        registered.runtimes.push_back(state);
        return Runtime(std::move(state));
    }
    ultralight::Renderer *native() const {
        state_->require_open();
        return state_->renderer.get();
    }
    void SetErrorHandler(std::function<void(const Error &)> callback) const {
        state_->require_open();
        state_->error_handler = std::move(callback);
    }
    void Update() const {
        drive([](auto &renderer) { renderer.Update(); });
    }
    void RefreshDisplay(std::uint32_t display = 0) const {
        drive([&](auto &renderer) { renderer.RefreshDisplay(display); });
    }
    void Render() const {
        drive([](auto &renderer) { renderer.Render(); });
    }
    // Thread-safe posting. Pending work is suppressed after Close. Captures must
    // themselves be safe to destroy on the SDK's task-destruction thread.
    void PostTask(std::function<void()> callback) const {
        auto state = state_;
        std::lock_guard lock(state->tasks_mutex);
        if (state->closed)
            throw Error(ErrorCode::Closed, "Runtime is closed");
        state->renderer->PostTask([weak = std::weak_ptr(state), callback = std::move(callback)] {
            auto state = weak.lock();
            if (!state || state->closed)
                return;
            try {
                if (callback)
                    callback();
            }
            catch (const Error &error) {
                state->report(error);
            }
            catch (const std::exception &error) {
                state->report(Error(ErrorCode::HostException, error.what()));
            }
            catch (...) {
                state->report(Error(ErrorCode::HostException, "Unknown posted task exception"));
            }
        });
    }
    ManagedView CreateView(std::uint32_t width, std::uint32_t height, const ultralight::ViewConfig &config,
                           ultralight::RefPtr<ultralight::Session> session = nullptr) const;
    void Close() const {
        auto state = state_;
        state->require_thread();
        if (state->closed)
            return;
        state->require_unlocked();
        if (state->external_host && !state->external_host->IsClosed()) {
            state->external_host->Close();
            return;
        }
        auto host_keepalive = state->host_keepalive;
        state->closed = true;
        auto views = std::exchange(state->views, {});
        for (auto &weak : views)
            if (auto view = weak.lock())
                view->close();
        ultralight::RefPtr<ultralight::Renderer> renderer;
        {
            std::lock_guard lock(state->tasks_mutex);
            renderer = std::exchange(state->renderer, {});
        }
        state->error_handler = {};
        state->host_keepalive.reset();
    }
};

class ManagedView {
    std::shared_ptr<runtime_detail::ViewState> state_;
    explicit ManagedView(std::shared_ptr<runtime_detail::ViewState> state) : state_(std::move(state)) {
    }
    friend class Runtime;
    friend class API;
    friend struct appcore_detail::Access;
    template <typename F> void input(F &&callback) const {
        auto state = state_;
        state->require_open();
        state->require_unlocked();
        auto runtime = state->runtime;
        runtime->require_unlocked();
        if (runtime->busy || runtime->bridge_scopes)
            throw Error(ErrorCode::Busy, "Input cannot be dispatched during Renderer, input, or Bridge calls");
        // A callback may close either wrapper. Keep both SDK objects alive until
        // dispatch returns, without retaining any language-specific references.
        auto host_keepalive = runtime->host_keepalive;
        auto renderer = runtime->renderer;
        auto view = state->view;
        runtime->busy = true;
        struct Guard {
            bool &busy;
            ~Guard() {
                busy = false;
            }
        } guard{runtime->busy};
        std::invoke(std::forward<F>(callback), *view.get());
    }

  public:
    // Borrowed escape hatch. Do not replace this wrapper's load/view listeners.
    ultralight::View *native() const {
        state_->require_open();
        return state_->view.get();
    }
    void FireMouseEvent(const ultralight::MouseEvent &event) const {
        input([&](auto &view) { view.FireMouseEvent(event); });
    }
    void FireScrollEvent(const ultralight::ScrollEvent &event) const {
        input([&](auto &view) { view.FireScrollEvent(event); });
    }
    void FireKeyEvent(const ultralight::KeyEvent &event) const {
        input([&](auto &view) { view.FireKeyEvent(event); });
    }
    void Focus() const {
        auto state = state_;
        input([&](auto &view) {
            auto host = state->host;
            if (host)
                host->Focus();
            else
                view.Focus();
        });
    }
    void Unfocus() const {
        auto state = state_;
        input([&](auto &view) {
            auto host = state->host;
            if (host)
                host->Unfocus();
            else
                view.Unfocus();
        });
    }
    bool HasFocus() const {
        auto state = state_;
        state->require_open();
        auto view = state->view;
        return view->HasFocus();
    }
    bool HasInputFocus() const {
        auto state = state_;
        state->require_open();
        auto view = state->view;
        return view->HasInputFocus();
    }
    void SetCallbacks(ViewCallbacks callbacks) const {
        state_->require_open();
        state_->callbacks = std::move(callbacks);
    }
    void LoadHTML(std::string_view html, std::string_view url = {}) const {
        auto state = state_;
        state->require_open();
        state->require_unlocked();
        auto host_keepalive = state->runtime->host_keepalive;
        auto view = state->view;
        view->LoadHTML(ultralight::String(html.data(), html.size()), ultralight::String(url.data(), url.size()));
    }
    void LoadURL(std::string_view url) const {
        auto state = state_;
        state->require_open();
        state->require_unlocked();
        auto host_keepalive = state->runtime->host_keepalive;
        auto view = state->view;
        view->LoadURL(ultralight::String(url.data(), url.size()));
    }
    void Resize(std::uint32_t width, std::uint32_t height) const {
        auto state = state_;
        state->require_open();
        state->require_unlocked();
        if (!width || !height)
            throw Error(ErrorCode::InvalidArgument, "View dimensions must be positive");
        auto host_keepalive = state->runtime->host_keepalive;
        auto view = state->view;
        view->Resize(width, height);
    }
    void Close() const {
        auto state = state_;
        state->runtime->require_thread();
        if (state->closed)
            return;
        state->require_unlocked();
        state->close();
    }
    // Return owned host data or a Bridge::Handle, never a borrowed JSValueRef.
    template <typename F> decltype(auto) WithBridge(F &&callback) const {
        auto state = state_;
        state->require_open();
        state->require_unlocked();
        auto host_keepalive = state->runtime->host_keepalive;
        auto view = state->view;
        auto runtime = state->runtime;
        ++runtime->bridge_scopes;
        struct Scope {
            std::size_t &count;
            ~Scope() {
                --count;
            }
        } scope{runtime->bridge_scopes};
        auto lock = view->LockJSContext();
        state->require_open();
        if (!lock.get() || !lock->ctx())
            throw Error(ErrorCode::PageGone, "View has no JavaScript context");
        if (!state->bridge || !state->bridge->matches(lock->ctx())) {
            state->expire();
            state->require_open();
            state->bridge = jsc::Bridge::create(lock->ctx(), view.get());
        }
        auto bridge = state->bridge;
        return std::invoke(std::forward<F>(callback), *bridge);
    }
    template <typename F> void WithPixels(F &&callback) const {
        auto state = state_;
        state->require_open();
        state->require_unlocked();
        if (state->runtime->busy)
            throw Error(ErrorCode::Busy, "Pixels may only be read outside Renderer calls");
        auto view = state->view;
        auto *surface = view->surface();
        if (!surface)
            throw Error(ErrorCode::Unsupported, "CPU pixels are unavailable for this View");
        auto width = surface->width(), height = surface->height(), stride = surface->row_bytes();
        auto size = surface->size();
        if (std::uint64_t(stride) < std::uint64_t(width) * 4 || std::uint64_t(stride) * height > size)
            throw Error(ErrorCode::InvalidArgument, "Invalid Surface dimensions or stride");
        ++state->pixel_locks;
        ++state->runtime->pixel_locks;
        struct Counts {
            runtime_detail::ViewState &state;
            ~Counts() {
                --state.pixel_locks;
                --state.runtime->pixel_locks;
            }
        } counts{*state};
        auto pixels = surface->LockPixelsSafe();
        if (!pixels.data())
            throw Error(ErrorCode::Busy, "Could not lock Surface pixels");
        std::invoke(std::forward<F>(callback), PixelView{width,
                                                         height,
                                                         stride,
                                                         PixelFormat::BGRA8Premultiplied,
                                                         surface->dirty_bounds(),
                                                         {static_cast<const std::byte *>(pixels.data()), size}});
    }
    PixelBuffer CopyPixels() const {
        PixelBuffer result;
        WithPixels([&](const PixelView &pixels) {
            result.width = pixels.width;
            result.height = pixels.height;
            result.row_bytes = pixels.row_bytes;
            result.format = pixels.format;
            result.dirty_bounds = pixels.dirty_bounds;
            result.data.assign(pixels.data.begin(), pixels.data.end());
        });
        return result;
    }
    void ClearDirtyBounds() const {
        state_->require_open();
        state_->require_unlocked();
        auto *surface = state_->view->surface();
        if (!surface)
            throw Error(ErrorCode::Unsupported, "CPU pixels are unavailable for this View");
        surface->ClearDirtyBounds();
    }
};

inline ManagedView Runtime::CreateView(std::uint32_t width, std::uint32_t height, const ultralight::ViewConfig &config,
                                       ultralight::RefPtr<ultralight::Session> session) const {
    auto state = state_;
    state->require_open();
    state->require_unlocked();
    if (!width || !height)
        throw Error(ErrorCode::InvalidArgument, "View dimensions must be positive");
    auto host_keepalive = state->host_keepalive;
    auto renderer = state->renderer;
    auto view = renderer->CreateView(width, height, config, std::move(session));
    if (!view.get())
        throw Error(ErrorCode::InvalidArgument, "CreateView failed");
    auto result = std::make_shared<runtime_detail::ViewState>(state, std::move(view));
    result->view->set_load_listener(result.get());
    result->view->set_view_listener(result.get());
    std::erase_if(state->views, [](const auto &weak) { return weak.expired(); });
    state->views.push_back(result);
    return ManagedView(std::move(result));
}
} // namespace ulbind17
