#pragma once
#include <AppCore/AppCore.h>
#include <AppCore/Platform.h>
#include <cmath>
#include <ulbind17/runtime.hpp>
#if defined(__APPLE__)
#include <pthread.h>
#endif

namespace ulbind17 {
namespace appcore_detail {
struct Access {
    static auto State(const Runtime &runtime) {
        return runtime.state_;
    }
    static auto State(const ManagedView &view) {
        return view.state_;
    }
    static Runtime Wrap(std::shared_ptr<runtime_detail::RuntimeState> state) {
        return Runtime(std::move(state));
    }
    static bool Initialized() {
        auto &registry = Runtime::registry();
        std::lock_guard lock(registry.mutex);
        return registry.initialized;
    }
};
inline void RequireMainThread() {
#if defined(__APPLE__)
    if (!pthread_main_np())
        throw Error(ErrorCode::WrongThread, "AppCore requires the main thread");
#endif
}
struct WindowState;
struct AppState final : runtime_detail::ExternalHost {
    const std::thread::id thread = std::this_thread::get_id();
    ultralight::RefPtr<ultralight::App> app;
    std::weak_ptr<runtime_detail::RuntimeState> runtime;
    std::vector<std::weak_ptr<WindowState>> windows;
    bool closed = false;
    explicit AppState(ultralight::RefPtr<ultralight::App> value) : app(std::move(value)) {
    }
    ~AppState() {
        Close();
    }
    bool IsClosed() const noexcept override {
        return closed;
    }
    void RequireThread() const {
        if (thread != std::this_thread::get_id())
            throw Error(ErrorCode::WrongThread, "AppCore operation requires the main thread");
    }
    void RequireOpen() const {
        RequireThread();
        if (closed)
            throw Error(ErrorCode::Closed, "Application is closed");
    }
    void Report(const Error &error) noexcept {
        if (auto state = runtime.lock())
            state->report(error);
    }
    void Close() noexcept override;
};
} // namespace appcore_detail

namespace appcore {
// Register before Renderer::Create()/Runtime::Create(). Other platform services are untouched.
inline void UseSystemFontLoader() {
    if (appcore_detail::Access::Initialized() || ultralight::App::instance())
        throw Error(ErrorCode::Busy, "Select the font loader before creating a Renderer or App");
    ultralight::Platform::instance().set_font_loader(ultralight::GetPlatformFontLoader());
}
struct WindowCallbacks {
    std::function<void()> closed;
    std::function<void(double, double)> resized; // Logical pixels.
};
class Window;
class Application;
} // namespace appcore

namespace appcore_detail {
struct WindowState final : ultralight::WindowListener, std::enable_shared_from_this<WindowState> {
    std::shared_ptr<AppState> application;
    // Even an empty Window must keep the Runtime alive after Application handles are dropped.
    std::shared_ptr<runtime_detail::RuntimeState> runtime;
    ultralight::RefPtr<ultralight::Window> window;
    std::vector<std::shared_ptr<runtime_detail::ViewState>> views;
    appcore::WindowCallbacks callbacks;
    bool closed = false;
    WindowState(std::shared_ptr<AppState> app, ultralight::RefPtr<ultralight::Window> value)
        : application(std::move(app)), runtime(application->runtime.lock()), window(std::move(value)) {
    }
    ~WindowState() {
        Close();
    }
    void RequireOpen() const {
        application->RequireThread();
        if (closed || application->closed)
            throw Error(ErrorCode::Closed, "Window is closed");
    }
    template <typename F> void Dispatch(F callback) noexcept {
        auto keep = weak_from_this().lock();
        try {
            callback();
        }
        catch (const Error &error) {
            application->Report(error);
        }
        catch (const std::exception &error) {
            application->Report(Error(ErrorCode::HostException, error.what()));
        }
        catch (...) {
            application->Report(Error(ErrorCode::HostException, "Unknown window callback exception"));
        }
    }
    void Close() noexcept {
        if (closed)
            return;
        closed = true;
        auto app = application->app;
        auto native = std::exchange(window, {});
        auto owned_views = std::exchange(views, {});
        for (auto &view : owned_views)
            view->close();
        if (native.get()) {
            native->set_listener(nullptr);
            native->Close();
        }
        auto previous = std::exchange(callbacks, {});
        Dispatch([&] {
            if (previous.closed)
                previous.closed();
        });
    }
    void OnClose(ultralight::Window *) override {
        auto keep = weak_from_this().lock();
        Close();
    }
    void OnResize(ultralight::Window *, double width, double height) override {
        if (closed || application->closed)
            return;
        Dispatch([&] {
            auto callback = callbacks.resized;
            if (callback)
                callback(width, height);
        });
    }
};
struct PanelHost final : runtime_detail::ViewHost {
    std::weak_ptr<WindowState> window;
    ultralight::RefPtr<ultralight::Panel> panel;
    PanelHost(std::shared_ptr<WindowState> owner, ultralight::RefPtr<ultralight::Panel> value)
        : window(std::move(owner)), panel(std::move(value)) {
    }
    void Focus() override {
        auto owner = window.lock();
        if (!owner || owner->closed || !panel->view().get())
            throw Error(ErrorCode::Closed, "Panel is closed");
        auto app = owner->application->app;
        auto native = owner->window;
        auto content = panel;
        content->Focus();
    }
    void Unfocus() override {
        auto owner = window.lock();
        if (!owner || owner->closed || !panel->view().get())
            throw Error(ErrorCode::Closed, "Panel is closed");
        auto app = owner->application->app;
        auto native = owner->window;
        if (native->focused_panel().get() == panel.get())
            native->ClearFocus();
    }
    void Close() noexcept override {
        if (!panel.get())
            return;
        // Panel handles don't own their native nodes or window. Remove explicitly.
        auto previous = std::exchange(panel, {});
        auto parent = previous->parent();
        if (parent.get())
            parent->Remove(previous);
    }
};
inline void AppState::Close() noexcept {
    if (closed)
        return;
    closed = true;
    auto native = std::exchange(app, {});
    if (native.get())
        native->Quit();
    auto owned_windows = std::exchange(windows, {});
    for (auto &weak : owned_windows)
        if (auto window = weak.lock())
            window->Close();
    if (auto state = runtime.lock()) {
        try {
            Access::Wrap(state).Close();
        }
        catch (const Error &error) {
            Report(error);
        }
    }
    // native is released after the Runtime's renderer reference and all windows.
}
} // namespace appcore_detail

namespace appcore {
class Window {
    std::shared_ptr<appcore_detail::WindowState> state_;
    explicit Window(std::shared_ptr<appcore_detail::WindowState> state) : state_(std::move(state)) {
    }
    friend class Application;

  public:
    // Borrowed. Do not replace the listener or mutate managed panel ownership through native().
    ultralight::Window *native() const {
        state_->RequireOpen();
        return state_->window.get();
    }
    void SetCallbacks(WindowCallbacks callbacks) const {
        state_->RequireOpen();
        state_->callbacks = std::move(callbacks);
    }
    ultralight::RefPtr<ultralight::Panel> AdoptPanel(const ManagedView &view,
                                                     const ultralight::PanelOptions &options = {}) const {
        auto owner = state_;
        owner->RequireOpen();
        auto managed = appcore_detail::Access::State(view);
        managed->require_open();
        managed->runtime->require_unlocked();
        if (managed->runtime != owner->application->runtime.lock())
            throw Error(ErrorCode::InvalidArgument, "Panel and View must share the Application's Runtime");
        if (managed->host)
            throw Error(ErrorCode::InvalidArgument, "View is already hosted by a Panel");
        if (managed->runtime->busy || managed->runtime->bridge_scopes)
            throw Error(ErrorCode::Busy, "Adopt a Panel outside Renderer and Bridge calls");
        if (managed->view->is_accelerated() && !owner->window->is_accelerated())
            throw Error(ErrorCode::Unsupported, "A CPU window cannot present an accelerated View");
        auto panel = owner->window->AdoptPanel(managed->view, options);
        if (!panel.get() || panel->view().get() != managed->view.get())
            throw Error(ErrorCode::InvalidArgument, "AppCore could not adopt the View");
        try {
            auto host = std::make_shared<appcore_detail::PanelHost>(owner, panel);
            std::erase_if(owner->views, [](const auto &entry) { return entry->closed; });
            owner->views.push_back(managed);
            managed->host = std::move(host);
        }
        catch (...) {
            owner->window->layout()->Remove(panel);
            // Removing a Panel clears these listeners even though ManagedView retains the View.
            managed->view->set_load_listener(managed.get());
            managed->view->set_view_listener(managed.get());
            throw;
        }
        return panel;
    }
    void Close() const {
        auto state = state_;
        state->application->RequireThread();
        if (state->closed)
            return;
        if (auto runtime = state->application->runtime.lock())
            runtime->require_unlocked();
        state->Close();
    }
};

class Application {
    Runtime runtime_;
    std::shared_ptr<appcore_detail::AppState> state_;
    Application(Runtime runtime, std::shared_ptr<appcore_detail::AppState> state)
        : runtime_(std::move(runtime)), state_(std::move(state)) {
    }
    struct Registry {
        std::weak_ptr<appcore_detail::AppState> application;
        bool initialized = false;
    };
    static Registry &registry() {
        static Registry value;
        return value;
    }

  public:
    static Application Create(ultralight::Settings settings = {}, ultralight::Config config = {}) {
        appcore_detail::RequireMainThread();
        auto &registered = registry();
        if (auto state = registered.application.lock()) {
            state->RequireOpen();
            auto runtime = appcore_detail::Access::Wrap(state->runtime.lock());
            return Application(std::move(runtime), std::move(state));
        }
        if (registered.initialized)
            throw Error(ErrorCode::Closed, "Application recreation is unsupported");
        if (appcore_detail::Access::Initialized() || ultralight::App::instance())
            throw Error(ErrorCode::InvalidArgument, "Create the Application before any other Renderer or App");
        auto &platform = ultralight::Platform::instance();
        if (platform.gpu_driver() ||
            (platform.surface_factory() && platform.surface_factory() != ultralight::GetBitmapSurfaceFactory()))
            throw Error(ErrorCode::Unsupported, "AppCore windows require AppCore's GPUDriver and SurfaceFactory");
        auto native = ultralight::App::Create(std::move(settings), std::move(config));
        if (!native.get())
            throw Error(ErrorCode::InvalidArgument, "AppCore creation failed");
        registered.initialized = true; // SDK App/Renderer recreation is unsafe even after a setup failure.
        auto runtime = Runtime::FromExisting(native->renderer());
        auto state = std::make_shared<appcore_detail::AppState>(std::move(native));
        state->runtime = appcore_detail::Access::State(runtime);
        appcore_detail::Access::State(runtime)->host_keepalive =
            std::make_shared<ultralight::RefPtr<ultralight::App>>(state->app);
        appcore_detail::Access::State(runtime)->external_host = state;
        registered.initialized = true;
        registered.application = state;
        return Application(std::move(runtime), std::move(state));
    }
    // Borrowed. Loop control and shutdown must go through this adapter.
    ultralight::App *native() const {
        state_->RequireOpen();
        return state_->app.get();
    }
    Runtime runtime() const {
        state_->RequireOpen();
        return runtime_;
    }
    Window CreateWindow(double width, double height, bool fullscreen = false,
                        ultralight::WindowFlags flags = ultralight::WindowFlags::Titled |
                                                        ultralight::WindowFlags::Resizable,
                        ultralight::Monitor *monitor = nullptr) const {
        state_->RequireOpen();
        auto runtime = appcore_detail::Access::State(runtime_);
        runtime->require_unlocked();
        if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0)
            throw Error(ErrorCode::InvalidArgument, "Window dimensions must be finite and positive");
        if (runtime->busy || runtime->bridge_scopes)
            throw Error(ErrorCode::Busy, "Create a Window outside Renderer and Bridge calls");
        auto window = ultralight::Window::Create(monitor ? monitor : state_->app->main_monitor(), width, height,
                                                 fullscreen, flags);
        if (!window.get())
            throw Error(ErrorCode::InvalidArgument, "Window creation failed");
        auto state = std::make_shared<appcore_detail::WindowState>(state_, std::move(window));
        state->window->set_listener(state.get());
        std::erase_if(state_->windows, [](const auto &entry) { return entry.expired(); });
        state_->windows.push_back(state);
        return Window(std::move(state));
    }
    bool RunOnce(double max_wait_seconds = 0.0) const {
        auto state = state_;
        state->RequireThread();
        if (state->closed)
            return false;
        if (!std::isfinite(max_wait_seconds) || max_wait_seconds < 0)
            throw Error(ErrorCode::InvalidArgument, "RunOnce wait must be finite and non-negative");
        auto runtime = appcore_detail::Access::State(runtime_);
        runtime->require_open();
        runtime->require_unlocked();
        if (runtime->busy || runtime->bridge_scopes)
            throw Error(ErrorCode::Busy, "AppCore calls cannot be nested or made inside Bridge calls");
        runtime->busy = true;
        struct Guard {
            bool &busy;
            ~Guard() {
                busy = false;
            }
        } guard{runtime->busy};
        auto native = state->app;
        auto renderer = runtime->renderer;
        bool running = native->RunOnce(max_wait_seconds);
        if (!running)
            state->Close();
        return running && !state->closed;
    }
    void Close() const {
        auto state = state_;
        state->RequireThread();
        if (state->closed)
            return;
        appcore_detail::Access::State(runtime_)->require_unlocked();
        state->Close();
    }
};
} // namespace appcore
} // namespace ulbind17
