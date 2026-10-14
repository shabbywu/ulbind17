#include "../common.hpp"
#include "../game_ui.hpp"

int main(int argc, char **argv) {
    return official::Main([&] {
        official::Host host(official::Options::Parse(argc, argv), "Official13GameUI");
        auto *monitor = host.app.native()->main_monitor();
        auto scale = std::min({1.0, monitor->width() * 0.8 / 1600, monitor->height() * 0.8 / 900});
        auto &page = host.CreatePage("ulbind17 / 13 — Game UI", 1600 * scale, 900 * scale);
        page.window.native()->MoveToCenter();
        page.Load(game_ui::URL(host.options.scene));
        host.Run({}, [&] {
            if (!game_ui::Ready(page.view, !host.options.scene.empty()))
                return false;
            if (!host.options.scene.empty())
                game_ui::CheckScene(page.view);
            return true;
        });
    });
}
