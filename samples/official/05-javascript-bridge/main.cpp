#include "../common.hpp"
#include <ulbind17/API.hpp>

int main(int argc, char **argv) {
    return official::Main([&] {
        official::Host host(official::Options::Parse(argc, argv), "Official05JavaScriptBridge");
        auto &page = host.CreatePage("ulbind17 / 05 — JavaScript Bridge", 520, 560);
        unsigned ticks = 0;
        bool reloaded = false;
        std::unique_ptr<official::Bridge::Handle> previous;
        ulbind17::API api("app");
        api["version"] = "2.0.0-beta.2 / ulbind17";
        api.Bind("ping", [](std::string message) { return "pong: " + message; });
        api.Bind("increment", [](double value) { return value + 1; });
        api.DefineEvent<double>("tick");
        official::Check(api.AttachTo(page.view), "API attachment failed");
        page.Load("file:///app.html");
        auto start = official::Clock::now(), last_tick = start;
        host.Run(
            [&] {
                if (!page.ready || official::Clock::now() - last_tick < std::chrono::seconds(1))
                    return;
                last_tick = official::Clock::now();
                api.Emit("tick", std::chrono::duration<double>(last_tick - start).count());
                ++ticks;
            },
            [&] {
                if (!ticks)
                    return false;
                official::Check(page.Boolean("app.ping('hello') === 'pong: hello' && app.increment(41) === 42"),
                                "Bridge round trip failed");
                if (page.String("document.getElementById('uptime').textContent") == "0:00")
                    return false; // SDK events are delivered during a later update.
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
