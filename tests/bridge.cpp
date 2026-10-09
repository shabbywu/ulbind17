#include "../samples/common.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>

namespace js = ulbind17::js;
using sample::check;
using sample::take;

void values() {
    sample::Fixture fixture;
    js::Context context(fixture.view.get());
    auto object = take(context.Evaluate(R"(
        globalThis.reads = 0;
        const obj = Object.create({ inherited: 7 });
        Object.defineProperty(obj, 'hidden', { value: 8 });
        Object.defineProperty(obj, 'getter', { enumerable: true, get() { ++reads; throw new Error('getter failed'); } });
        obj['六'] = true;
        obj.number = '7';
        obj;
    )"));
    check(take(ulbind17::keys(object)) == std::vector<std::string>({"getter", "六", "number"}),
          "keys must contain only own enumerable string properties");
    check(take(ulbind17::size(object)) == 3, "object size failed");
    check(take(context.Evaluate<int>("reads")) == 0, "key enumeration ran a getter");
    auto getter = ulbind17::get<int>(object, "getter");
    check(!getter && getter.error().message().find("getter failed") != std::string::npos,
          "getter exception was lost");
    auto wrong = ulbind17::get<int>(object, "number");
    check(!wrong, "strict conversion must not coerce strings to numbers");
    auto missing = ulbind17::get<int>(object, "missing");
    check(!missing, "missing numeric property must fail");
    check(take(ulbind17::get<bool>(object, "六")), "UTF-8 key failed");
    auto empty = ulbind17::keys(js::Value {});
    check(!empty && empty.error().is_empty(), "empty-handle error was lost");
    auto primitive = ulbind17::size(context.Make(3));
    check(!primitive && primitive.error().type() == js::ErrorType::TypeError, "primitive size must fail");
    auto fractional = context.Make(3.5).To<int>();
    check(!fractional, "fractional integer conversion must fail");
    auto indexed = context.MakeArray({context.Make(1)});
    indexed[-1] = 9;
    check(take(ulbind17::get<int>(indexed, -1)) == 9, "negative index must address a named property");
}

struct Tracked {
    inline static int alive = 0;
    int number = 0;
    Tracked() { ++alive; }
    ~Tracked() { --alive; }
    int add(int amount) { return number += amount; }
};

void lifetime() {
    sample::Fixture fixture;
    js::Context context(fixture.view.get());
    {
        js::API api("temporary");
        api["add"] = [](int a, int b) { return a + b; };
        check(api.AttachTo(fixture.view.get()), "AttachTo failed");
        take(context.Evaluate("globalThis.saved = temporary.add"));
        check(take(context.Evaluate<int>("saved(2, 3)")) == 5, "live callback failed");
    }
    auto dead = context.Evaluate("saved(2, 3)");
    check(!dead && dead.error().code() == "ULJS_DETACHED", "destroyed API must detach callbacks");

    Tracked borrowed;
    js::API api("classes");
    api.DefineClass<Tracked>("Tracked").Constructor<>().Field("number", &Tracked::number).Method("add", &Tracked::add);
    check(api.AttachTo(fixture.view.get()), "class AttachTo failed");
    auto owner = take(context.Evaluate("new classes.Tracked()"));
    check(Tracked::alive == 2, "native constructor did not create an owned instance");
    auto reclaimed = js::Detach<Tracked>(owner);
    check(reclaimed.get() != nullptr, "Detach must return ownership of an owned instance");
    reclaimed.reset();
    check(Tracked::alive == 1, "owned instance was not destroyed exactly once");
    auto failed = owner["add"].Invoke<int>(1);
    check(!failed && failed.error().code() == "ULJS_DETACHED", "owned wrapper must detach");
    auto wrapper = context.Make(&borrowed);
    check(take(wrapper["add"].Invoke<int>(2)) == 2 && borrowed.number == 2, "borrowed instance failed");
    auto not_owned = js::Detach<Tracked>(wrapper);
    check(!not_owned && Tracked::alive == 1, "detaching a borrowed instance must not delete it");
    auto detached = wrapper["add"].Invoke<int>(1);
    check(!detached && detached.error().code() == "ULJS_DETACHED", "borrowed wrapper must detach");
}

struct LoadWaiter : ultralight::LoadListener {
    ultralight::View *view;
    bool ready = false;
    explicit LoadWaiter(ultralight::View *view) : view(view) { view->set_load_listener(this); }
    ~LoadWaiter() { view->set_load_listener(nullptr); }
    void OnDOMReady(ultralight::View *, unsigned long long, bool main_frame, const ultralight::String &) override {
        if (main_frame)
            ready = true;
    }
    void wait(ultralight::Renderer *renderer) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!ready && std::chrono::steady_clock::now() < deadline) {
            renderer->Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(ready, "page did not reach DOMReady");
    }
};

void navigation() {
    sample::Fixture fixture;
    js::API api("app");
    api["add"] = [](int a, int b) { return a + b; };
    check(api.AttachTo(fixture.view.get()), "AttachTo failed");
    js::Context old_context(fixture.view.get());
    auto old_value = old_context.Make(12);
    LoadWaiter waiter(fixture.view.get());
    fixture.view->LoadHTML("<script>globalThis.answer = app.add(2,3)</script><body>First page</body>");
    waiter.wait(fixture.renderer.get());
    js::Context current(fixture.view.get());
    check(take(current.Evaluate<int>("answer")) == 5, "API was not injected before page script");
    auto stale = old_value.To<int>();
    check(!stale && stale.error().is_page_gone(), "old-page values must expire after navigation");
    auto old_eval = old_context.Evaluate("1");
    check(!old_eval && old_eval.error().is_page_gone(), "old-page context must expire");
    auto stale_keys = ulbind17::keys(old_value);
    check(!stale_keys && stale_keys.error().is_page_gone(), "helper lost page-gone error");
    waiter.ready = false;
    fixture.view->LoadHTML("<script>globalThis.answer = app.add(4,5)</script><body>Second page</body>");
    waiter.wait(fixture.renderer.get());
    js::Context next(fixture.view.get());
    check(take(next.Evaluate<int>("answer")) == 9, "API did not survive a second navigation");
}

void resources(bool chinese) {
    check(!std::filesystem::exists("assets/resources"), "resource test requires no external runtime data");
    auto *fs = ultralight::Platform::instance().file_system();
    for (const auto *name : {"icudt67l.dat", "cacert.pem"}) {
        std::string path = std::string("resources/") + name;
        check(fs->FileExists(path.c_str()), "embedded resource not found: " + path);
        auto buffer = fs->OpenFile(path.c_str());
        check(buffer.get() != nullptr, "embedded resource cannot be opened");
        std::ifstream input(std::filesystem::path(ULBIND17_SDK_RESOURCES) / name, std::ios::binary);
        std::vector<char> expected((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        check(buffer->size() == expected.size() &&
                  std::equal(expected.begin(), expected.end(), static_cast<const char *>(buffer->data())),
              "embedded resource differs from the selected SDK: " + path);
    }
    auto root = std::filesystem::path(chinese ? "disk-chinese" : "disk-ascii");
    std::filesystem::create_directories(root);
    { std::ofstream(root / "content.txt", std::ios::binary) << "root-relative file"; }
    ulbind17::platform::FileSystem disk(root);
    auto content = disk.OpenFile("content.txt");
    check(content.get() && std::string(static_cast<const char *>(content->data()), content->size()) == "root-relative file",
          "FileSystem OpenFile must use the same root as FileExists");

    sample::Fixture fixture;
    LoadWaiter waiter(fixture.view.get());
    fixture.view->LoadHTML(chinese ? "<body style='font-size:28px;color:red'>中文字体测试</body>"
                                 : "<body style='font-size:28px;color:red'>Embedded font</body>");
    waiter.wait(fixture.renderer.get());
    fixture.renderer->RefreshDisplay(0);
    fixture.renderer->Render();
    auto *surface = static_cast<ultralight::BitmapSurface *>(fixture.view->surface());
    check(surface && !surface->dirty_bounds().IsEmpty(), "CPU page was not rendered");
    auto bitmap = surface->bitmap();
    const auto *pixels = static_cast<const unsigned char *>(bitmap->LockPixels());
    bool text = false;
    for (std::size_t y = 0; y < bitmap->height() && !text; ++y) {
        for (std::size_t x = 0; x < bitmap->width(); ++x) {
            const auto *pixel = pixels + y * bitmap->row_bytes() + x * 4;
            if (pixel[2] > pixel[0] + 20 && pixel[2] > pixel[1] + 20) {
                text = true;
                break;
            }
        }
    }
    bitmap->UnlockPixels();
    check(text, "embedded font did not produce colored text pixels");
}

int main(int argc, char **argv) {
    std::string mode = argc > 1 ? argv[1] : "values";
    return sample::run([&] {
        if (mode == "values") values();
        else if (mode == "lifetime") lifetime();
        else if (mode == "navigation") navigation();
        else if (mode == "resources" || mode == "chinese") resources(mode == "chinese");
        else throw std::runtime_error("unknown test case");
    }, mode == "chinese");
}
