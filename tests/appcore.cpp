#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <ulbind17/appcore.hpp>
#include <ulbind17/platform/FileSystem.hpp>

using namespace ulbind17;
namespace ac = ulbind17::appcore;
#if defined(__APPLE__)
bool ResizeAppCoreTestWindow(const char *, double, double);
#endif
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <typename F> void expect(ErrorCode code, F callback) {
    try {
        callback();
    }
    catch (const Error &error) {
        check(error.code == code, "Unexpected error category");
        return;
    }
    throw std::runtime_error("Expected error was not raised");
}
struct Logger final : ultralight::Logger {
    std::mutex mutex;
    std::vector<std::string> messages;
    void LogMessage(ultralight::LogLevel, const ultralight::String &message) override {
        std::lock_guard lock(mutex);
        auto text = message.utf8();
        messages.emplace_back(text.data(), text.length());
    }
};
template <typename F> void pump(const ac::Application &app, F complete) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!complete()) {
        check(std::chrono::steady_clock::now() < deadline, "AppCore operation timed out");
        check(app.RunOnce(0.002) || complete(), "AppCore stopped unexpectedly");
    }
}
void fonts() {
    platform::FileSystem files(ULBIND17_TEST_SDK_ROOT);
    Logger logger;
    auto &platform = ultralight::Platform::instance();
    platform.set_file_system(&files);
    platform.set_logger(&logger);
    ac::UseSystemFontLoader();
    ac::UseSystemFontLoader();
    check(platform.file_system() == &files && platform.logger() == &logger, "Font selection replaced custom services");
    auto *fonts = platform.font_loader();
    check(fonts == ultralight::GetPlatformFontLoader(), "System font loader was not installed");
    check(fonts->Load(fonts->fallback_font(), 400, false).get(), "System fallback font did not load");
    check(fonts->Load(fonts->fallback_font_for_characters("中文", 400, false), 400, false).get(),
          "System Chinese fallback font did not load");
    auto runtime = Runtime::Create();
    expect(ErrorCode::Busy, [] { ac::UseSystemFontLoader(); });
    expect(ErrorCode::InvalidArgument, [] { ac::Application::Create(); });
    ultralight::ViewConfig config;
    auto view = runtime.CreateView(200, 100, config);
    bool ready = false;
    ViewCallbacks callbacks;
    callbacks.dom_ready = [&](const auto &event) {
        if (event.is_main_frame)
            ready = true;
    };
    view.SetCallbacks(callbacks);
    view.LoadHTML("<body style='font-family: sans-serif'>System fonts 中文</body>");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!ready) {
        check(std::chrono::steady_clock::now() < deadline, "Font-only page timed out");
        runtime.Update();
        runtime.RefreshDisplay();
        runtime.Render();
    }
    runtime.Close();
}
void paths() {
    std::filesystem::create_directories("first");
    std::filesystem::create_directories("second");
    {
        std::ofstream file("first/only.txt");
        file << "first";
    }
    auto *files = ultralight::GetPlatformFileSystem("first");
    check(files == ultralight::GetPlatformFileSystem("second"), "FileSystem is not a singleton");
    check(files->FileExists("only.txt") && files->OpenFile("only.txt").get(), "First filesystem path did not win");
    auto *logger = ultralight::GetDefaultLogger("first/first.log");
    check(logger == ultralight::GetDefaultLogger("second/second.log"), "Logger is not a singleton");
    logger->LogMessage(ultralight::LogLevel::Info, "AppCore path test");
    check(std::filesystem::exists("first/first.log") && !std::filesystem::exists("second/second.log"),
          "Later log path replaced the first path");
}
void hosted(std::string_view mode) {
    platform::FileSystem files(ULBIND17_TEST_SDK_ROOT);
    Logger logger;
    auto &platform = ultralight::Platform::instance();
    platform.set_file_system(&files);
    platform.set_logger(&logger);
    ultralight::Settings settings;
    settings.developer_name = "ulbind17";
    settings.app_name = "AppCoreTests";
    settings.force_cpu_renderer = mode != "gpu";
    settings.headless_paint_fps = 60;
    if (mode == "scale-1")
        settings.device_scale_override = 1;
    if (mode == "scale-2")
        settings.device_scale_override = 2;
    if (mode == "custom-surface") {
        struct CustomSurfaceFactory final : ultralight::SurfaceFactory {
            ultralight::Surface *CreateSurface(uint32_t, uint32_t) override {
                return nullptr;
            }
            void DestroySurface(ultralight::Surface *) override {
            }
        } factory;
        platform.set_surface_factory(&factory);
        expect(ErrorCode::Unsupported, [&] { ac::Application::Create(settings); });
        check(platform.surface_factory() == &factory, "Rejected initialization replaced custom surface factory");
        platform.set_surface_factory(ultralight::GetBitmapSurfaceFactory());
    }
    if (mode == "window-owner") {
        std::optional<ac::Window> survivor;
        ultralight::App *native = nullptr;
        {
            auto temporary = ac::Application::Create(settings);
            native = temporary.native();
            survivor.emplace(temporary.CreateWindow(100, 100, false, ultralight::WindowFlags::Hidden));
        }
        auto recovered = ac::Application::Create();
        check(recovered.native() == native, "Empty Window did not keep Application alive");
        auto runtime = recovered.runtime();
        auto view = runtime.CreateView(100, 100, survivor->native()->default_view_config());
        survivor->AdoptPanel(view);
        recovered.RunOnce();
        recovered.Close();
        expect(ErrorCode::Closed, [&] { survivor->native(); });
        expect(ErrorCode::Closed, [&] { view.native(); });
        return;
    }
    auto app = ac::Application::Create(settings);
    auto runtime = app.runtime();
    auto repeated = ac::Application::Create();
    check(repeated.native() == app.native(), "Application singleton was not shared");
    check(platform.file_system() == &files && platform.logger() == &logger, "App replaced custom services");
    check(platform.font_loader() == ultralight::GetPlatformFontLoader(), "App did not use system fonts");
    auto shared_runtime = Runtime::FromExisting(app.native()->renderer());
    expect(ErrorCode::Unsupported, [&] { runtime.Update(); });
    expect(ErrorCode::Unsupported, [&] { shared_runtime.Render(); });
    expect(ErrorCode::Unsupported, [&] { Runtime::Create().RefreshDisplay(); });
    expect(ErrorCode::InvalidArgument, [&] { app.RunOnce(-1); });
    expect(ErrorCode::InvalidArgument, [&] { app.CreateWindow(0, 100); });
    auto window = app.CreateWindow(320, 200, false,
                                   ultralight::WindowFlags::Titled | ultralight::WindowFlags::Resizable |
                                       ultralight::WindowFlags::Hidden);
    auto config = window.native()->default_view_config();
    auto view = runtime.CreateView(window.native()->device_width(), window.native()->device_height(), config);
    auto *load = view.native()->load_listener();
    auto *events = view.native()->view_listener();
    auto panel = window.AdoptPanel(view);
    check(view.native()->load_listener() == load && view.native()->view_listener() == events,
          "Adoption replaced ManagedView listeners");
    auto *editor = view.native()->editor_listener();
    check(editor, "AppCore editor listener is missing");
    expect(ErrorCode::InvalidArgument, [&] { window.AdoptPanel(view); });
    int errors = 0;
    runtime.SetErrorHandler([&](const Error &error) {
        check(error.code == ErrorCode::HostException, "Lost host exception");
        ++errors;
    });
    int ready = 0, children = 0;
    bool failed = false;
    ViewCallbacks callbacks;
    callbacks.window_ready = [&](const auto &event) {
        if (!event.is_main_frame) {
            ++children;
            return;
        }
        expect(ErrorCode::Busy, [&] { app.RunOnce(); });
        view.WithBridge([](auto &bridge) {
            bridge.bindFunc("echo", [](JSContextRef context, JSObjectRef, std::span<const JSValueRef> args) {
                return args.empty() ? JSValueMakeUndefined(context) : args[0];
            });
        });
    };
    callbacks.dom_ready = [&](const auto &event) {
        if (!event.is_main_frame)
            return;
        ++ready;
        if (mode == "callback-close") {
            view.Close();
            throw std::runtime_error("Expected callback failure");
        }
        if (mode == "callback-app-close")
            app.Close();
        if (mode == "callback-window-close")
            window.Close();
        if (mode == "callback-runtime-close")
            runtime.Close();
    };
    callbacks.fail_loading = [&](const auto &failure) {
        failed = true;
        check(!failure.domain.empty(), "Missing load diagnostics");
    };
    view.SetCallbacks(callbacks);
    view.LoadHTML("<body><input id='input'><script>globalThis.answer=echo(42)</script>"
                  "<iframe srcdoc='<script>1</script><body>child</body>'></iframe></body>");
    pump(app, [&] { return ready > 0; });
    if (mode == "destruction") {
        std::optional<ManagedView> survivor;
        {
            auto transient = app.CreateWindow(100, 100, false, ultralight::WindowFlags::Hidden);
            survivor.emplace(runtime.CreateView(100, 100, transient.native()->default_view_config()));
            transient.AdoptPanel(*survivor);
            { auto copy = transient; }
            check(survivor->native(), "Dropping one Window handle closed the shared Window");
        }
        expect(ErrorCode::Closed, [&] { survivor->native(); });
        view.Close();
        app.Close();
        return;
    }
    if (mode == "native-close") {
        window.native()->Close();
        expect(ErrorCode::Closed, [&] { view.native(); });
        expect(ErrorCode::Closed, [&] { window.native(); });
        app.Close();
        return;
    }
    if (mode.starts_with("callback-")) {
        expect(ErrorCode::Closed, [&] { view.native(); });
        if (mode == "callback-close")
            check(errors == 1, "Callback exception escaped or was lost");
        if (mode == "callback-app-close" || mode == "callback-runtime-close")
            check(!app.RunOnce(), "Closed app still runs");
        app.Close();
        return;
    }
    pump(app, [&] { return children > 0; });
    view.WithBridge([&](auto &bridge) {
        check(bridge.readNumber(bridge.evaluateRetained("answer")->value()) == 42, "First-line binding was late");
        expect(ErrorCode::Busy, [&] { app.RunOnce(); });
    });
    bool task_ran = false;
    std::thread worker([&] {
        expect(ErrorCode::WrongThread, [&] { app.RunOnce(); });
        expect(ErrorCode::WrongThread, [&] { window.Close(); });
        runtime.PostTask([&] { task_ran = true; });
    });
    worker.join();
    pump(app, [&] { return task_ran; });
    view.Focus();
    check(window.native()->focused_panel().get() == panel.get(), "ManagedView focus bypassed Panel");
    view.Unfocus();
    check(!window.native()->focused_panel().get(), "Unfocus did not clear Window focus");
    check(view.native()->editor_listener() == editor, "Focus replaced the editor listener");
    if (mode.starts_with("scale-") || mode == "gpu" || mode == "lifecycle") {
        pump(app, [&] { return panel->bounds().height() > 0; });
        auto scale = window.native()->scale();
        if (mode == "scale-1")
            check(scale == 1, "Scale override 1 was ignored");
        if (mode == "scale-2")
            check(scale == 2, "Scale override 2 was ignored");
        check(view.native()->width() == window.native()->device_width(), "Panel device width is wrong");
        check(view.native()->height() == window.native()->device_height(), "Panel device height is wrong");
        view.WithBridge([&](auto &bridge) {
            check(std::abs(bridge.readNumber(bridge.evaluateRetained("devicePixelRatio")->value()) - scale) < 0.01,
                  "Page DPI differs from Window scale");
        });
        auto other = runtime.CreateView(100, 100, config);
        auto second = window.AdoptPanel(other);
        other.Focus();
        view.Unfocus();
        check(window.native()->focused_panel().get() == second.get(), "Unfocus cleared another Panel's focus");
        pump(app, [&] { return second->bounds().height() > 0; });
        auto previous = panel->bounds().height();
        panel->SetSize("40px");
        pump(app, [&] { return std::abs(panel->bounds().height() - previous) > 1; });
        auto old = view.WithBridge([](auto &bridge) { return bridge.evaluateRetained("()=>42"); });
        bool cancelled = false;
        view.WithBridge([&](auto &bridge) {
            auto promise = bridge.evaluateRetained("new Promise(()=>{})");
            bridge.then(promise->value(), [](JSValueRef, bool) {}, [&] { cancelled = true; });
        });
        int closed = 0;
        window.SetCallbacks({[&] {
                                 ++closed;
                                 expect(ErrorCode::Closed, [&] { view.native(); });
                                 throw std::runtime_error("Expected window exception");
                             },
                             {}});
        window.Close();
        window.Close();
        check(closed == 1 && errors == 1 && cancelled, "Window close callbacks or Promise cancellation failed");
        expect(ErrorCode::PageGone, [&] { old->value(); });
        expect(ErrorCode::Closed, [&] { other.native(); });
        check(!panel->view().get() && !second->view().get(), "Closed Window retained Panels");
    } else if (mode == "navigation") {
        auto old = view.WithBridge([](auto &bridge) { return bridge.evaluateRetained("()=>42"); });
        view.LoadHTML("<script>globalThis.answer=echo(43)</script>");
        pump(app, [&] { return ready > 1; });
        expect(ErrorCode::PageGone, [&] { old->value(); });
        view.WithBridge([](auto &bridge) {
            check(bridge.readNumber(bridge.evaluateRetained("answer")->value()) == 43,
                  "Binding was not installed after navigation");
        });
        view.LoadURL("file:///missing-appcore-test.html");
        pump(app, [&] { return failed; });
        view.Close();
        check(!panel->view().get(), "View close did not remove Panel");
        check(window.native(), "View close closed its Window");
#if defined(__APPLE__)
    } else if (mode == "native-resize") {
        int resizes = 0;
        double logical_width = 0, logical_height = 0;
        window.native()->SetTitle("ulbind17-native-resize-test");
        window.SetCallbacks({{}, [&](double width, double height) {
                                 ++resizes;
                                 logical_width = width;
                                 logical_height = height;
                             }});
        check(ResizeAppCoreTestWindow("ulbind17-native-resize-test", 480, 280), "Native test Window was not found");
        pump(app, [&] {
            return resizes > 0 && view.native()->width() == window.native()->device_width() &&
                   view.native()->height() == window.native()->device_height() && panel->bounds().width() == 480 &&
                   panel->bounds().height() == 280;
        });
        check(logical_width == 480 && logical_height == 280, "Window resize callback did not use logical pixels");
        view.WithBridge([](auto &bridge) {
            check(bridge.readNumber(bridge.evaluateRetained("innerWidth")->value()) == 480,
                  "Page did not resize with the native Window");
        });
#endif
    } else if (mode == "pixel-lock") {
        view.WithPixels([&](const auto &) {
            expect(ErrorCode::Busy, [&] { app.RunOnce(); });
            expect(ErrorCode::Busy, [&] { app.Close(); });
            expect(ErrorCode::Busy, [&] { window.Close(); });
            expect(ErrorCode::Busy, [&] { runtime.Close(); });
        });
    } else if (mode == "bridge-close") {
        view.WithBridge([&](auto &bridge) {
            bridge.bindFunc("shutdown", [&](JSContextRef context, JSObjectRef, std::span<const JSValueRef>) {
                app.Close();
                return JSValueMakeUndefined(context);
            });
            expect(ErrorCode::PageGone, [&] { bridge.evaluateRetained("shutdown()"); });
        });
        check(!app.RunOnce(), "Synchronous Bridge shutdown did not close App");
        return;
    } else if (mode == "input-close") {
        int editable_changes = 0;
        auto editor_callback =
            window.native()->OnEditableStateChange([&](const ultralight::EditableState &) { ++editable_changes; });
        view.Focus();
        view.WithBridge([](auto &bridge) {
            bridge.evaluateRetained(
                "document.getElementById('input').focus();"
                "document.getElementById('input').addEventListener('input',()=>console.log('shutdown'))");
        });
        check(editable_changes > 0, "AppCore editor listener did not forward editable-state changes");
        ViewCallbacks closing;
        closing.console = [&](const auto &) { app.Close(); };
        view.SetCallbacks(std::move(closing));
        ultralight::KeyEvent key;
        key.type = ultralight::KeyEvent::kType_Char;
        key.text = "文";
        key.unmodified_text = "文";
        view.FireKeyEvent(key);
        check(!app.RunOnce(), "Shutdown during text input did not close App");
        expect(ErrorCode::Closed, [&] { view.native(); });
        return;
    } else if (mode == "focus-close") {
        auto registration = window.native()->OnFocusChange([&](ultralight::Panel *focused) {
            if (focused)
                app.Close();
        });
        view.Focus();
        check(!app.RunOnce(), "Close in native focus callback did not stop app");
        return;
    }
    bool suppressed = true;
    runtime.PostTask([&] { suppressed = false; });
    if (mode == "runtime-close")
        runtime.Close();
    else
        app.Close();
    app.Close();
    runtime.Close();
    check(!app.RunOnce() && suppressed, "Closed app executed pending work");
    expect(ErrorCode::Closed, [&] { shared_runtime.native(); });
    expect(ErrorCode::Closed, [&] { repeated.native(); });
    expect(ErrorCode::Closed, [&] { ac::Application::Create(); });
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::string_view mode = argc > 1 ? argv[1] : "lifecycle";
        if (mode == "fonts")
            fonts();
        else if (mode == "paths")
            paths();
        else
            hosted(mode);
        std::cout << "AppCore " << mode << " passed\n";
        return 0;
    }
    catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
