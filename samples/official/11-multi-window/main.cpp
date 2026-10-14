#include "../common.hpp"
#include <ulbind17/API.hpp>

int main(int argc, char **argv) {
    return official::Main([&] {
        official::Host host(official::Options::Parse(argc, argv), "Official11MultiWindow");
        auto &preview = host.CreatePage("ulbind17 / 11 — Live Preview", 600, 700);
        auto &editor = host.CreatePage("ulbind17 / 11 — HTML Editor", 600, 700);
        editor.window.native()->MoveTo(50, 50);
        preview.window.native()->MoveTo(700, 50);
        unsigned updates = 0;
        ulbind17::API api("native");
        api.Bind("updateEditor", [&](std::string html) {
            preview.ready = false;
            preview.view.LoadHTML(html);
            ++updates;
        });
        official::Check(api.AttachTo(editor.view), "API attachment failed");
        preview.Load("file:///preview.html");
        editor.Load("file:///editor.html");
        bool sent = false;
        host.Run({}, [&] {
            if (!updates)
                return false; // Wait for the editor to send its initial document.
            if (!sent) {
                official::Check(preview.Boolean("document.body.textContent.includes('Hello, Ultralight!')"),
                                "Initial editor content was not previewed");
                editor.Execute("const input = document.getElementById('editor');"
                               "input.value = '<h1 id=\"smoke\">ManagedView preview</h1>';"
                               "input.dispatchEvent(new Event('input', {bubbles: true}));");
                sent = true;
                return false;
            }
            return updates >= 2 &&
                   preview.Boolean("document.getElementById('smoke')?.textContent === 'ManagedView preview'");
        });
    });
}
