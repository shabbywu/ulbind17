#include "../samples/common.hpp"
#include <filesystem>
#include <fstream>
#include <optional>
#include <ulbind17/API.hpp>

using namespace ulbind17;
using sample::check;
namespace {
template <typename F> void expect(ErrorCode code, F callback) {
    try {
        callback();
    }
    catch (const Error &error) {
        check(error.code == code, error.what());
        return;
    }
    throw std::runtime_error("Expected structured error");
}
template <typename F> void pump(const Runtime &runtime, F ready) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!ready() && std::chrono::steady_clock::now() < deadline) {
        runtime.Update();
        runtime.RefreshDisplay();
        runtime.Render();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(ready(), "API test timed out");
}
struct Page {
    ManagedView view;
    unsigned loads = 0;
    explicit Page(const Runtime &runtime) : view(runtime.CreateView(160, 100, Config())) {
        ViewCallbacks callbacks;
        callbacks.dom_ready = [this](const auto &event) {
            if (event.is_main_frame)
                ++loads;
        };
        view.SetCallbacks(std::move(callbacks));
    }
    static ultralight::ViewConfig Config() {
        ultralight::ViewConfig value;
        value.is_accelerated = false;
        return value;
    }
    bool test(std::string_view script) {
        return view.WithBridge(
            [&](auto &bridge) { return bridge.readBoolean(bridge.evaluateRetained(script)->value()); });
    }
    void load(const Runtime &runtime, std::string_view html) {
        auto previous = loads;
        view.LoadHTML(html);
        pump(runtime, [&] { return loads > previous; });
    }
};
struct Counter {
    int value = 0;
    int Add(int amount) {
        return value += amount;
    }
};

void registration(const Runtime &runtime) {
    Page first(runtime), second(runtime);
    API api("app", {.diagnostics = js::Diagnostics::Strict});
    api.Bind("ping", [](std::string text) { return "pong: " + text; }, js::Param("message"));
    api.Bind("fail", []() -> int { throw std::runtime_error("native failure"); });
    api.Bind("integer", [](int value) { return value; });
    api["version"] = "test";
    api.DefineEvent<int>("tick");
    api.DefineClass<Counter>("Counter").Constructor<>().Field("value", &Counter::value).Method("add", &Counter::Add);
    check(api.AttachTo(first.view) && api.AttachTo(first.view) && api.AttachTo(second.view), "API attachment failed");
    const char *html =
        "<script>globalThis.first=app.ping('hello');globalThis.ticks=0;app.on('tick',v=>ticks+=v)</script>";
    first.load(runtime, html);
    second.load(runtime, html);
    check(first.test("first==='pong: hello' && app.version==='test' && Object.isFrozen(app)"),
          "First script injection failed");
    check(first.test("(()=>{let c=new app.Counter();c.value=4;return c.add(3)===7})()"), "Class binding failed");
    check(first.test("(()=>{try{app.ping(1);return false}catch(e){return e instanceof TypeError}})()"),
          "Argument mismatch failed");
    check(first.test("(()=>{try{app.integer(1.5);return false}catch(e){return e.code==='ULJS_LOSSY'}})()"),
          "Strict numeric conversion failed");
    check(first.test("(()=>{try{app.fail();return false}catch(e){return e instanceof Error && String(e).includes('the "
                     "native code failed')}})()"),
          "Native exception failed");
    std::thread emitter([&] { api.Emit("tick", 2); });
    emitter.join();
    pump(runtime, [&] { return first.test("ticks===2") && second.test("ticks===2"); });
    auto previous = first.view.WithBridge([](auto &bridge) { return bridge.evaluateRetained("app.ping"); });
    first.load(runtime, html);
    expect(ErrorCode::PageGone, [&] { previous->value(); });
    check(first.test("first==='pong: hello'"), "Navigation did not reinject API");
    api.DetachFrom(first.view);
    check(first.test("app.ping('x')==='pong: x'"), "Detach changed current function semantics");
    api.Emit("tick", 3);
    pump(runtime, [&] { return second.test("ticks===5"); });
    check(first.test("ticks===0"), "Detached page received an event");
    first.load(runtime, "<body>detached</body>");
    check(first.test("typeof app==='undefined'"), "Detached API was reinjected");
    check(api.AttachTo(first.view), "Reattachment failed");
    first.load(runtime, html);
    auto native = ultralight::RefPtr<ultralight::View>(first.view.native());
    first.view.Close();
    api.Emit("tick", 4);
    pump(runtime, [&] { return second.test("ticks===9"); });
    js::Context context(native.get());
    check(sample::take(context.Evaluate<int>("ticks")) == 0, "Closed managed page retained event attachment");
    runtime.Close(); // Close callbacks before Page state disappears.
}

void lifetime(const Runtime &runtime) {
    Page page(runtime);
    auto api = std::make_unique<API>("app");
    api->Bind("ping", [] { return 42; });
    check(api->AttachTo(page.view), "API attachment failed");
    page.load(runtime, "<script>globalThis.saved=app.ping</script>");
    api.reset();
    check(page.test("(()=>{try{saved();return false}catch(e){return e.code==='ULJS_DETACHED'}})()"),
          "Borrowed ref kept API owner alive");
    page.load(runtime, "<body></body>");
    check(page.test("typeof app==='undefined'"), "Destroyed API reinjected");
    runtime.Close();
}

void constraints(const Runtime &runtime) {
    Page page(runtime);
    API api("app"), invalid("ul");
    check(!invalid.AttachTo(page.view), "Invalid API was attached");
    bool wrong = false;
    std::thread worker([&] {
        try {
            (void)api.AttachTo(page.view);
        }
        catch (const Error &error) {
            wrong = error.code == ErrorCode::WrongThread;
        }
    });
    worker.join();
    check(wrong, "Attachment accepted wrong thread");
    page.load(runtime, "<body>pixels</body>");
    runtime.Render();
    page.view.WithPixels([&](const auto &) {
        expect(ErrorCode::Busy, [&] { (void)api.AttachTo(page.view); });
        expect(ErrorCode::Busy, [&] { api.DetachFrom(page.view); });
    });
    page.view.Close();
    expect(ErrorCode::Closed, [&] { (void)api.AttachTo(page.view); });
    expect(ErrorCode::Closed, [&] { api.DetachFrom(page.view); });
}

void frames(const Runtime &runtime) {
    Page page(runtime);
    API api("app");
    api.Bind("ping", [] { return 42; });
    auto html = "<iframe srcdoc=\"<script>parent.childType=typeof app;if(typeof "
                "app!=='undefined')parent.childValue=app.ping()</script>\"></iframe>";
    check(api.AttachTo(page.view), "Default frame attachment failed");
    page.load(runtime, html);
    pump(runtime, [&] { return page.test("typeof childType!=='undefined'"); });
    check(page.test("childType==='undefined'"), "Main-frame default injected into child");
    check(api.AttachTo(page.view, {.flags = js::AllFrames}), "All-frame attachment failed");
    page.load(runtime, html);
    pump(runtime, [&] { return page.test("typeof childValue!=='undefined'"); });
    check(page.test("childValue===42"), "All-frame injection failed");
    runtime.Close();
}

void callback_close(const Runtime &runtime) {
    Page page(runtime);
    bool invoked = false;
    API api("app");
    api.Bind("shutdown", [&] {
        invoked = true;
        page.view.Close();
    });
    check(api.AttachTo(page.view), "Close callback attachment failed");
    page.view.LoadHTML("<script>app.shutdown()</script>");
    pump(runtime, [&] { return invoked; });
    expect(ErrorCode::Closed, [&] { page.view.native(); });
    runtime.Close();
}

void origins(const Runtime &runtime) {
    Page page(runtime);
    API api("app");
    api.Bind("ping", [] { return 42; });
    check(api.AttachTo(page.view, {.origin_rules = {"https://allowed.example"}}), "Origin attachment failed");
    page.load(runtime, "<script>globalThis.available=typeof app</script>");
    check(page.test("available==='undefined'"), "Explicit rules failed to exclude local content");
    auto previous = page.loads;
    page.view.LoadHTML("<script>globalThis.value=app.ping()</script>", "https://allowed.example/index.html");
    pump(runtime, [&] { return page.loads > previous; });
    check(page.test("value===42"), "Allowed origin was withheld");
    check(api.AttachTo(page.view), "Default origin attachment failed");
    page.load(runtime, "<script>globalThis.value=app.ping()</script>");
    check(page.test("value===42"), "Default local origin withheld");
    runtime.Close();
}
} // namespace
int main(int argc, char **argv) {
    return sample::run([&] {
        auto runtime = Runtime::Create();
        std::string name = argc > 1 ? argv[1] : "registration";
        if (name == "registration")
            registration(runtime);
        else if (name == "lifetime")
            lifetime(runtime);
        else if (name == "constraints")
            constraints(runtime);
        else if (name == "origins")
            origins(runtime);
        else if (name == "frames")
            frames(runtime);
        else if (name == "callback-close")
            callback_close(runtime);
        else
            throw std::runtime_error("Unknown API test");
        runtime.Close();
    });
}
