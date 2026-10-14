#include "../common.hpp"

int main(int argc, char **argv) {
    return official::Main([&] {
        official::Host host(official::Options::Parse(argc, argv), "Official07FileLoading");
        auto &page = host.CreatePage("ulbind17 / 07 — File Loading", 450, 700);
        page.bind = [](auto &bridge) {
            bridge.evaluateRetained(R"JS(
                addEventListener('DOMContentLoaded', () => {
                    const image = new Image();
                    globalThis.__sampleBackground = image;
                    image.onload = () => { globalThis.__backgroundReady = image.naturalWidth > 0; };
                    image.onerror = () => __sampleErrors.push('background.jpg did not decode');
                    image.src = 'background.jpg';
                });
            )JS");
        };
        page.Load("file:///app.html");
        host.Run({}, [&] {
            return page.Boolean(
                       "globalThis.__backgroundReady === true && document.getElementById('time').textContent.length > "
                       "0 && document.styleSheets.length > 0") &&
                   ultralight::Platform::instance().file_system()->OpenFile("background.jpg").get();
        });
    });
}
