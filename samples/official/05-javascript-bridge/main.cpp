#include "../common.hpp"

int main(int argc, char **argv) {
    return official::Main([&] {
        official::Host host(official::Options::Parse(argc, argv), "Official05JavaScriptBridge");
        auto &page = host.CreatePage("ulbind17 / 05 — JavaScript Bridge", 520, 560);
        unsigned ticks = 0;
        bool reloaded = false;
        std::unique_ptr<official::Bridge::Handle> previous;
        page.bind = [&](auto &bridge) {
            auto weak = std::weak_ptr(bridge.shared_from_this());
            bridge.bindFunc("__nativePing", [weak](JSContextRef, JSObjectRef, std::span<const JSValueRef> args) {
                auto bridge = weak.lock();
                official::Check(bool(bridge) && args.size() == 1, "ping expects one string");
                return bridge->makeString("pong: " + bridge->readString(args[0]));
            });
            bridge.bindFunc("__nativeIncrement", [weak](JSContextRef, JSObjectRef, std::span<const JSValueRef> args) {
                auto bridge = weak.lock();
                official::Check(bool(bridge) && args.size() == 1, "increment expects one number");
                return bridge->makeNumber(bridge->readNumber(args[0]) + 1);
            });
            bridge.evaluateRetained(R"JS(
                const ticks = new Set();
                globalThis.__emitTick = value => { for (const callback of ticks) callback(value); };
                globalThis.app = Object.freeze({
                    version: '2.0.0-beta.2 / ulbind17',
                    ping: __nativePing, increment: __nativeIncrement,
                    on(name, callback) {
                        if (name !== 'tick' || typeof callback !== 'function') throw new TypeError('Unknown event');
                        ticks.add(callback); return () => ticks.delete(callback);
                    }
                });
            )JS");
        };
        page.Load("file:///app.html");
        auto start = official::Clock::now(), last_tick = start;
        host.Run(
            [&] {
                if (!page.ready || official::Clock::now() - last_tick < std::chrono::seconds(1))
                    return;
                last_tick = official::Clock::now();
                page.view.WithBridge([&](auto &bridge) {
                    auto emit = bridge.evaluateRetained("__emitTick");
                    auto value =
                        bridge.retain(bridge.makeNumber(std::chrono::duration<double>(last_tick - start).count()));
                    const official::Bridge::Handle *args[] = {value.get()};
                    bridge.callRetained(*emit, args);
                });
                ++ticks;
            },
            [&] {
                if (!ticks)
                    return false;
                official::Check(page.Boolean("app.ping('hello') === 'pong: hello' && app.increment(41) === 42"),
                                "Bridge round trip failed");
                official::Check(page.String("document.getElementById('uptime').textContent") != "0:00",
                                "Native tick was not delivered");
                if (!reloaded) {
                    previous = page.view.WithBridge([](auto &bridge) { return bridge.evaluateRetained("app.ping"); });
                    reloaded = true;
                    ticks = 0;
                    page.Load("file:///app.html");
                    return false;
                }
                bool expired = false;
                try {
                    previous->value();
                }
                catch (const ulbind17::Error &error) {
                    expired = error.code == ulbind17::ErrorCode::PageGone;
                }
                official::Check(expired, "Navigation retained the previous Bridge");
                return true;
            });
    });
}
