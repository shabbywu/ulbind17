#include "../common.hpp"

int main(int argc, char **argv) {
    return official::Main([&] {
        official::Host host(official::Options::Parse(argc, argv), "Official02BasicApp");
        auto &page = host.CreatePage("ulbind17 / 02 — Basic App", 900, 600);
        page.Load("file:///page.html");
        host.Run({}, [&] {
            return page.Boolean("document.body.children.length > 0") && page.window.native()->is_visible();
        });
    });
}
