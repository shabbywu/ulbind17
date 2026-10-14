#include <chrono>
#include <iostream>
#include <ulbind17/appcore.hpp>

int main(int argc, char **argv) {
    bool smoke = false, cpu = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view argument = argv[i];
        if (argument == "--smoke-test")
            smoke = true;
        else if (argument == "--cpu")
            cpu = true;
        else {
            std::cerr << "Usage: AppCoreExample [--smoke-test] [--cpu]\n";
            return 1;
        }
    }
    try {
        ultralight::Settings settings;
        settings.developer_name = "ulbind17";
        settings.app_name = "AppCoreExample";
        settings.force_cpu_renderer = cpu;
        auto app = ulbind17::appcore::Application::Create(settings);
        auto runtime = app.runtime();
        bool failed = false, ready = false;
        runtime.SetErrorHandler([&](const auto &error) {
            failed = true;
            std::cerr << error.what() << '\n';
        });
        auto window = app.NewWindow(800, 540);
        window.native()->SetTitle("ulbind17 AppCore — system fonts, input and DPI");
        window.SetCallbacks({[&] { app.Close(); },
                             [&](double width, double height) {
                                 std::cout << "resize logical=" << width << 'x' << height
                                           << " scale=" << window.native()->scale() << '\n';
                             }});
        auto view = runtime.CreateView(window.native()->device_width(), window.native()->device_height(),
                                       window.native()->default_view_config());
        auto panel = window.AdoptPanel(view);
        ulbind17::ViewCallbacks callbacks;
        callbacks.window_ready = [&](const auto &event) {
            if (!event.is_main_frame)
                return;
            view.WithBridge([](auto &bridge) {
                bridge.bindFunc("nativeEcho", [](JSContextRef context, JSObjectRef, std::span<const JSValueRef> args) {
                    return args.empty() ? JSValueMakeUndefined(context) : args[0];
                });
            });
        };
        callbacks.dom_ready = [&](const auto &event) {
            if (event.is_main_frame)
                ready = true;
        };
        callbacks.fail_loading = [&](const auto &failure) {
            failed = true;
            std::cerr << failure.description << '\n';
        };
        callbacks.request_close = [&] { window.Close(); };
        view.SetCallbacks(std::move(callbacks));
        view.LoadHTML(R"HTML(
<!doctype html><meta charset="utf-8"><title>AppCore integration</title>
<style>
body { margin: 32px; background: #f4f6fb; color: #182238; font: 17px -apple-system, sans-serif; }
h1 { font-size: 28px; } .card { padding: 20px; background: white; border-radius: 12px; margin: 16px 0; }
input, textarea { box-sizing: border-box; width: 100%; font: inherit; padding: 10px; border: 1px solid #abb5c8; border-radius: 6px; }
.mono { font-family: Menlo, monospace; } #metrics { color: #435578; }
</style>
<h1>ulbind17 + AppCore</h1>
<p>System fonts: English · 中文系统字体 · 日本語 · 한글</p>
<div class="card"><p class="mono">Menlo / monospace: 0123456789 ABCDEF</p><p id="metrics"></p></div>
<div class="card"><label>Keyboard and IME <input placeholder="在这里输入中文，验证 IME"></label>
<p><textarea rows="2" placeholder="Try selection, copy/paste and multiline text"></textarea></p></div>
<p id="bridge"></p><p>Resize the window or move it between displays to check layout and DPI.</p>
<script>
globalThis.bridgeAnswer = nativeEcho(42);
document.getElementById('bridge').textContent = 'Bridge: nativeEcho(42) = ' + bridgeAnswer;
function metrics() {
 document.getElementById('metrics').textContent = 'CSS: ' + innerWidth + ' × ' + innerHeight +
   ' · devicePixelRatio: ' + devicePixelRatio;
}
addEventListener('resize', metrics); metrics();
</script>
)HTML");
        view.Focus();
        // Software OpenGL under Xvfb can spend tens of seconds compiling the first frame.
#if defined(__linux__)
        constexpr int smoke_seconds = 90;
#else
        constexpr int smoke_seconds = 10;
#endif
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(smoke_seconds);
        int ready_iterations = 0;
        bool validated = false;
        while (app.RunOnce(0.002)) {
            if (failed)
                throw std::runtime_error("AppCore page failed");
            if (smoke) {
                if (std::chrono::steady_clock::now() > deadline)
                    throw std::runtime_error("AppCore smoke timed out: DOM=" + std::to_string(ready) +
                                             " panel_height=" + std::to_string(panel->bounds().height()) +
                                             " visible=" + std::to_string(window.native()->is_visible()) +
                                             " ready_iterations=" + std::to_string(ready_iterations));
                if (ready && panel->bounds().height() > 0 && ++ready_iterations >= 20) {
                    view.WithBridge([&](auto &bridge) {
                        if (bridge.readNumber(bridge.evaluateRetained("bridgeAnswer")->value()) != 42)
                            throw std::runtime_error("Bridge result is incorrect");
                        auto scale = bridge.readNumber(bridge.evaluateRetained("devicePixelRatio")->value());
                        if (std::abs(scale - window.native()->scale()) > 0.01)
                            throw std::runtime_error("Page DPI differs from Window scale");
                    });
                    if (!window.native()->is_visible())
                        throw std::runtime_error("Window is not visible");
                    std::cout << "AppCore smoke passed: " << (window.native()->is_accelerated() ? "GPU" : "CPU")
                              << " scale=" << window.native()->scale() << " device=" << view.native()->width() << 'x'
                              << view.native()->height() << '\n';
                    validated = true;
                    app.Close();
                }
            }
        }
        app.Close();
        if (smoke && !validated)
            throw std::runtime_error("Window closed before smoke validation completed");
        return 0;
    }
    catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
