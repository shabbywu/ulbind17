#include <ulbind17/setup.hpp>
#include <ulbind17/runtime.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>

// Stand-in for a Python/Godot adapter: the host owns its data and conversion rules.
int main() {
    try {
        ulbind17::setup_ultralight_platform();
        auto runtime = ulbind17::Runtime::Create();
        ultralight::ViewConfig config; config.is_accelerated = false;
        auto view = runtime.CreateView(320, 180, config);
        bool ready = false, settled = false;
        int score = 7;
        ulbind17::ViewCallbacks callbacks;
        callbacks.window_ready = [&](const ulbind17::FrameEvent &event) {
            if (!event.is_main_frame) return;
            view.WithBridge([&](auto &bridge) {
                bridge.bindFunc("echo", [](JSContextRef, JSObjectRef, std::span<const JSValueRef> args) {
                    return args.empty() ? nullptr : args[0];
                });
                ulbind17::jsc::Bridge::ObjectHooks hooks;
                hooks.get = [&](JSContextRef ctx, JSStringRef name) -> JSValueRef {
                    return ulbind17::jsc::utf8(name) == "score" ? JSValueMakeNumber(ctx, score) : nullptr;
                };
                hooks.set = [&](JSContextRef ctx, JSStringRef name, JSValueRef value) {
                    if (ulbind17::jsc::utf8(name) != "score") return false;
                    if (!JSValueIsNumber(ctx, value)) throw std::runtime_error("score must be a number");
                    auto number = JSValueToNumber(ctx, value, nullptr);
                    if (!std::isfinite(number) || std::floor(number) != number ||
                        number < (std::numeric_limits<int>::min)() || number > (std::numeric_limits<int>::max)())
                        throw std::runtime_error("score must fit a host integer");
                    score = static_cast<int>(number); return true;
                };
                hooks.has = [](JSContextRef, JSStringRef name) { return ulbind17::jsc::utf8(name) == "score"; };
                hooks.keys = [](JSContextRef) { return std::vector<std::string>{"score"}; };
                ulbind17::jsc::String name("host");
                bridge.bind(name, bridge.makeObject(&score, std::move(hooks)));
            });
        };
        callbacks.dom_ready = [&](const ulbind17::FrameEvent &event) { if (event.is_main_frame) ready = true; };
        view.SetCallbacks(std::move(callbacks));
        view.LoadHTML("<body style='background:#204060'><script>host.score=21; echo(host.score)</script>");
        auto pump = [&](auto complete) {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!complete() && std::chrono::steady_clock::now() < deadline) {
                runtime.Update(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (!complete()) throw std::runtime_error("Timed out");
        };
        pump([&] { return ready; });
        auto function = view.WithBridge([](auto &bridge) { return bridge.evaluateRetained("()=>host.score*2"); });
        view.WithBridge([&](auto &bridge) {
            auto result = bridge.callRetained(*function);
            if (bridge.readNumber(result->value()) != 42) throw std::runtime_error("Host call failed");
            auto promise = bridge.evaluateRetained("Promise.resolve(42)");
            bridge.then(promise->value(), [&](JSValueRef, bool rejected) {
                if (rejected) throw std::runtime_error("Promise rejected");
                settled = true;
            });
        });
        pump([&] { return settled; });
        runtime.RefreshDisplay(); runtime.Render();
        auto pixels = view.CopyPixels();
        std::cout << "Copied " << pixels.width << 'x' << pixels.height << " BGRA pixels\n";
        view.ClearDirtyBounds();
        ready = false;
        view.LoadHTML("<body>new document</body>");
        pump([&] { return ready; });
        bool expired = false;
        try { function->value(); }
        catch (const ulbind17::Error &error) {
            if (error.code != ulbind17::ErrorCode::PageGone) throw;
            expired = true;
            std::cout << error.what() << '\n';
        }
        if (!expired) throw std::runtime_error("Old handle remained valid");
        function.reset(); runtime.Close();
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
