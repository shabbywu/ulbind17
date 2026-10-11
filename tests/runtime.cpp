#include "../samples/common.hpp"
#include <ulbind17/runtime.hpp>
#include <atomic>
#include <optional>

using sample::check;
namespace jsc = ulbind17::jsc;
using ulbind17::Error;
using ulbind17::ErrorCode;

template <typename F> void expect(ErrorCode code, F callback) {
    try { callback(); }
    catch (const Error &error) { check(error.code == code, error.what()); return; }
    throw std::runtime_error("Expected structured error was not thrown");
}
template <typename F> void pump(const ulbind17::Runtime &runtime, F ready) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready() && std::chrono::steady_clock::now() < deadline) {
        runtime.Update(); runtime.RefreshDisplay(); runtime.Render(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(ready(), "Timed out waiting for page event");
}
ulbind17::ManagedView page(const ulbind17::Runtime &runtime) {
    ultralight::ViewConfig config; config.is_accelerated = false;
    auto view = runtime.CreateView(32, 24, config);
    bool ready = false;
    ulbind17::ViewCallbacks callbacks;
    callbacks.dom_ready = [&](const auto &event) { if (event.is_main_frame) ready = true; };
    view.SetCallbacks(callbacks); view.LoadHTML("<body></body>");
    pump(runtime, [&] { return ready; });
    view.SetCallbacks({}); return view;
}

void dynamic(const ulbind17::Runtime &runtime) {
    auto view = page(runtime);
    auto other = page(runtime);
    auto foreign = other.WithBridge([](auto &bridge) { return bridge.evaluateRetained("7"); });
    view.WithBridge([&](auto &bridge) {
        std::string nul("a\0b", 3);
        auto target = bridge.retain(bridge.makePlainObject());
        bridge.set(target->value(), nul, bridge.makeString(nul));
        check(bridge.readString(bridge.get(target->value(), nul)) == nul, "NUL key or string truncated");
        check(bridge.has(target->value(), nul) && !bridge.has(target->value(), "missing"), "has() failed");
        check(bridge.keys(target->value()) == std::vector<std::string>{nul}, "NUL enumeration failed");
        check(bridge.readBoolean(bridge.makeBoolean(true)), "Boolean roundtrip failed");
        check(JSValueIsNull(bridge.context(), bridge.makeNull()) &&
              JSValueIsUndefined(bridge.context(), bridge.makeUndefined()), "Null/undefined collapsed");
        expect(ErrorCode::TypeMismatch, [&] { bridge.readNumber(bridge.makeString("7")); });
        expect(ErrorCode::TypeMismatch, [&] { bridge.readBoolean(bridge.makeNumber(1)); });
        expect(ErrorCode::TypeMismatch, [&] { bridge.readString(bridge.makeNull()); });
        auto array = bridge.retain(bridge.makeArray());
        bridge.setAt(array->value(), 3, bridge.makeNumber(9));
        check(bridge.length(array->value()) == 4 && bridge.readNumber(bridge.at(array->value(), 3)) == 9,
              "Sparse array length or index wrong");
        check(JSValueIsUndefined(bridge.context(), bridge.at(array->value(), 0)), "Hole is not undefined");
        auto bad_length = bridge.evaluateRetained("new Proxy([], {get(o,k){return k==='length'?-1:Reflect.get(o,k)}})");
        expect(ErrorCode::TypeMismatch, [&] { bridge.length(bad_length->value()); });
        const std::string unicode = "\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x90\x89";
        bridge.set(target->value(), unicode, bridge.makeString(unicode));
        check(bridge.readString(bridge.get(target->value(), unicode)) == unicode, "Unicode property roundtrip failed");
        auto object = bridge.evaluateRetained("Object.create({inherited:1},{own:{value:2,enumerable:true},"
            "secret:{value:3},getme:{enumerable:true,get(){throw new Error('getter failed')}}})");
        check(bridge.keys(object->value()) == std::vector<std::string>({"own", "getme"}), "Own enumeration read getter");
        check(bridge.has(object->value(), "inherited"), "has should include inherited properties");
        expect(ErrorCode::ScriptException, [&] { bridge.get(object->value(), "getme"); });
        auto holder = bridge.evaluateRetained("({n:7,add:function(x){return this.n+x}})");
        auto function = bridge.getRetained(*holder, "add");
        auto number = bridge.retain(bridge.makeNumber(5));
        const jsc::Bridge::Handle *args[] = {number.get()};
        auto result = bridge.callRetained(*function, args, holder.get());
        check(bridge.readNumber(result->value()) == 12, "Method receiver was lost");
        const jsc::Bridge::Handle *foreign_args[] = {foreign.get()};
        expect(ErrorCode::CrossContext, [&] { bridge.callRetained(*function, foreign_args); });
        expect(ErrorCode::CrossContext, [&] { bridge.getRetained(*foreign, "x"); });
        expect(ErrorCode::CrossContext, [&] { bridge.callRetained(*function, {}, foreign.get()); });
        try { bridge.evaluateRetained("throw new Error('diagnostic')"); throw std::logic_error("Expected script exception"); }
        catch (const Error &error) {
            check(error.code == ErrorCode::ScriptException && std::string(error.what()).find("diagnostic") != std::string::npos,
                  "Exception message lost");
            check(!error.stack.empty(), "Exception stack lost");
        }
        std::vector<std::string> reads;
        jsc::Bridge::ObjectHooks hooks;
        hooks.get = [&](JSContextRef ctx, JSStringRef key) -> JSValueRef {
            reads.push_back(jsc::utf8(key));
            return reads.back() == "score" ? JSValueMakeNumber(ctx, 42) : nullptr;
        };
        hooks.has = [](JSContextRef, JSStringRef key) { return jsc::utf8(key) == "score"; };
        hooks.keys = [](JSContextRef) { return std::vector<std::string>{"score"}; };
        auto proxy = bridge.retain(bridge.makeObject(42, std::move(hooks)));
        check(bridge.template unwrap<int>(proxy->value()) && *bridge.template unwrap<int>(proxy->value()) == 42, "Extended proxy identity lost");
        check(bridge.keys(proxy->value()) == std::vector<std::string>{"score"}, "Proxy keys missing");
        check(reads.empty(), "Proxy enumeration invoked getter");
        check(bridge.has(proxy->value(), "score") && !bridge.has(proxy->value(), "missing"), "Proxy existence failed");
        check(bridge.readNumber(bridge.get(proxy->value(), "score")) == 42, "Proxy read failed");
        reads.clear();
        jsc::Bridge::ObjectHooks keys_only;
        keys_only.get = [&](JSContextRef ctx, JSStringRef) { reads.push_back("read"); return JSValueMakeNumber(ctx, 7); };
        keys_only.keys = [](JSContextRef) { return std::vector<std::string>{"own"}; };
        auto keys_proxy = bridge.retain(bridge.makeObject(7, std::move(keys_only)));
        check(bridge.keys(keys_proxy->value()) == std::vector<std::string>{"own"} && reads.empty(),
              "Keys-only proxy enumeration invoked getter");
        jsc::Traversal traversal(2);
        int a, b, c;
        { auto first = traversal.enter(&a);
          expect(ErrorCode::TraversalLimit, [&] { auto cycle = traversal.enter(&a); });
          { auto second = traversal.enter(&b);
            expect(ErrorCode::TraversalLimit, [&] { auto limit = traversal.enter(&c); }); }
          auto sibling = traversal.enter(&b);
        }
        auto clean = traversal.enter(&a);
        jsc::Traversal default_limit;
        int identities[65];
        std::function<void(int)> walk = [&](int depth) {
            if (depth == 65) return;
            auto guard = default_limit.enter(&identities[depth]); walk(depth + 1);
        };
        expect(ErrorCode::TraversalLimit, [&] { walk(0); });
        auto after_unwind = default_limit.enter(&identities[0]);
    });
    // Reentrant navigation invalidates a property read, even if its getter returns a value.
    view.WithBridge([&](auto &bridge) {
        bridge.bindFunc("navigate", [&](JSContextRef ctx, JSObjectRef, std::span<const JSValueRef>) {
            view.LoadHTML("<body>replaced</body>"); return JSValueMakeNumber(ctx, 1);
        });
        auto object = bridge.evaluateRetained("({get value(){navigate();return 3}})");
        expect(ErrorCode::PageGone, [&] { bridge.get(object->value(), "value"); });
    });
}

void lifecycle(const ulbind17::Runtime &runtime) {
    auto view = page(runtime);
    bool cancelled = false;
    int errors = 0;
    runtime.SetErrorHandler([&](const Error &error) {
        check(error.code == ErrorCode::HostException, "Host callback error category lost"); ++errors;
    });
    auto handle = view.WithBridge([&](auto &bridge) {
        auto promise = bridge.evaluateRetained("new Promise(()=>{})");
        bridge.then(promise->value(), [](JSValueRef, bool) { throw std::runtime_error("Unexpected settlement"); },
                    [&] { cancelled = true; view.Close(); });
        return bridge.evaluateRetained("()=>42");
    });
    view.Close(); view.Close();
    check(cancelled, "Close did not cancel Promise");
    expect(ErrorCode::Closed, [&] { view.native(); });
    expect(ErrorCode::PageGone, [&] { handle->value(); });
    std::thread releaser([handle = std::move(handle)]() mutable { handle.reset(); }); releaser.join();

    auto callback_view = page(runtime);
    bool ready = false;
    int finish_count = 0;
    ulbind17::ViewCallbacks callbacks;
    callbacks.dom_ready = [&](const auto &event) {
        if (!event.is_main_frame) return;
        ready = true; callback_view.Close(); throw std::runtime_error("callback failure");
    };
    callbacks.finish_loading = [&](const auto &) { ++finish_count; };
    callback_view.SetCallbacks(callbacks); callback_view.LoadHTML("<body>close in callback</body>");
    pump(runtime, [&] { return ready; });
    for (int i = 0; i < 3; ++i) runtime.Update();
    check(errors == 1 && finish_count == 0, "Closed view dispatched callbacks or exception escaped");

    auto navigation = page(runtime);
    int main_ready = 0, child_ready = 0;
    std::unique_ptr<jsc::Bridge::Handle> main_handle;
    ulbind17::ViewCallbacks nav_callbacks;
    nav_callbacks.window_ready = [&](const auto &event) {
        if (event.is_main_frame) {
            navigation.WithBridge([&](auto &bridge) {
                main_handle = bridge.evaluateRetained("()=>7");
                bridge.bindFunc("firstLine", [](JSContextRef ctx, JSObjectRef, std::span<const JSValueRef>) {
                    return JSValueMakeNumber(ctx, 42);
                });
            });
        } else ++child_ready;
    };
    nav_callbacks.dom_ready = [&](const auto &event) { if (event.is_main_frame) ++main_ready; };
    navigation.SetCallbacks(nav_callbacks);
    navigation.LoadHTML("<body><script>globalThis.answer=firstLine()</script>"
                        "<iframe srcdoc='<script>1</script>'></iframe></body>");
    pump(runtime, [&] { return main_ready > 0 && child_ready > 0; });
    navigation.WithBridge([&](auto &bridge) {
        check(bridge.readNumber(bridge.evaluateRetained("answer")->value()) == 42, "First-line binding was late");
        auto answer = bridge.callRetained(*main_handle);
        check(bridge.readNumber(answer->value()) == 7, "Child frame expired main bridge");
    });
    auto stale = std::move(main_handle);
    navigation.LoadHTML("<body>second</body>");
    pump(runtime, [&] { return main_ready > 1; });
    expect(ErrorCode::PageGone, [&] { stale->value(); });

    bool failed = false, finished = false;
    ulbind17::ViewCallbacks failure_callbacks;
    failure_callbacks.fail_loading = [&](const ulbind17::LoadFailure &failure) {
        failed = true; check(!failure.description.empty() && !failure.domain.empty(), "Load diagnostics missing");
    };
    failure_callbacks.finish_loading = [&](const auto &) { finished = true; };
    navigation.SetCallbacks(failure_callbacks);
    navigation.LoadURL("file:///ulbind17-missing-test-file.html");
    pump(runtime, [&] { return failed && finished; });
    runtime.SetErrorHandler({});
}

void threading(const ulbind17::Runtime &runtime) {
    auto view = page(runtime);
    auto handle = view.WithBridge([](auto &bridge) { return bridge.evaluateRetained("()=>42"); });
    view.WithBridge([&](auto &bridge) {
        std::exception_ptr worker_failure;
        std::thread worker([&] {
            try {
                expect(ErrorCode::WrongThread, [&] { bridge.makeString("worker"); });
                expect(ErrorCode::WrongThread, [&] { bridge.evaluateRetained("42"); });
            } catch (...) { worker_failure = std::current_exception(); }
        });
        worker.join(); if (worker_failure) std::rethrow_exception(worker_failure);
    });
    std::atomic<bool> posted = false;
    std::exception_ptr failure;
    std::thread worker([&] {
        try {
            expect(ErrorCode::WrongThread, [&] { runtime.Update(); });
            expect(ErrorCode::WrongThread, [&] { view.Resize(5, 5); });
            expect(ErrorCode::WrongThread, [&] { handle->value(); });
            runtime.PostTask([&] { posted = true; });
            handle.reset();
        } catch (...) { failure = std::current_exception(); }
    });
    worker.join(); if (failure) std::rethrow_exception(failure);
    pump(runtime, [&] { return posted.load(); });
    auto repeated = ulbind17::Runtime::Create();
    check(repeated.native() == runtime.native(), "Create did not reuse Renderer");
    auto adopted = ulbind17::Runtime::FromExisting(ultralight::RefPtr<ultralight::Renderer>(runtime.native()));
    auto native = ultralight::RefPtr<ultralight::Renderer>(runtime.native());
    bool queued = false; runtime.PostTask([&] { queued = true; });
    adopted.Close(); runtime.Close();
    native->Update(); check(!queued, "Task ran after Runtime closed");
    expect(ErrorCode::Closed, [&] { view.native(); });
    expect(ErrorCode::Closed, [&] { runtime.Render(); });
    expect(ErrorCode::Closed, [&] { runtime.PostTask([] {}); });
    expect(ErrorCode::Closed, [&] { ulbind17::Runtime::Create(); });
}


void input_events(const ulbind17::Runtime &runtime, bool close_view = false, bool close_runtime = false) {
    auto view = page(runtime); view.Resize(320, 240);
    bool ready = false;
    ulbind17::ViewCallbacks callbacks;
    callbacks.dom_ready = [&](const auto &event) { if (event.is_main_frame) ready = true; };
    view.SetCallbacks(callbacks);
    view.LoadHTML("<style>body{margin:0}button,input{display:block;width:150px;height:40px}</style>"
                  "<button id=b onclick='hit()'>click</button><input id=i>"
                  "<div style='height:2000px;width:2000px'>scroll</div>"
                  "<script>globalThis.clicks=0;function hit(){clicks++}</script>");
    pump(runtime, [&] { return ready; }); view.SetCallbacks({});
    runtime.Render();
    auto number = [&](const char *script) {
        return view.WithBridge([&](auto &bridge) {
            return bridge.readNumber(bridge.evaluateRetained(script)->value());
        });
    };
    auto click = [&] {
        view.FireMouseEvent({ultralight::MouseEvent::kType_MouseMoved, 20, 20, ultralight::MouseEvent::kButton_None});
        view.FireMouseEvent({ultralight::MouseEvent::kType_MouseDown, 20, 20, ultralight::MouseEvent::kButton_Left});
        view.FireMouseEvent({ultralight::MouseEvent::kType_MouseUp, 20, 20, ultralight::MouseEvent::kButton_Left});
    };
    std::exception_ptr worker_error;
    std::thread worker([&] {
        try { expect(ErrorCode::WrongThread, [&] { view.Focus(); }); }
        catch (...) { worker_error = std::current_exception(); }
    });
    worker.join(); if (worker_error) std::rethrow_exception(worker_error);
    view.Unfocus(); check(!view.HasFocus(), "Unfocus failed");
    view.Focus(); check(view.HasFocus(), "Focus failed");
    expect(ErrorCode::Busy, [&] { view.WithPixels([&](const auto &) { view.Focus(); }); });
    view.WithBridge([&](auto &) { expect(ErrorCode::Busy, [&] { view.Focus(); }); });
    click(); check(number("clicks") == 1, "Mouse click did not reach DOM");
    view.FireMouseEvent({ultralight::MouseEvent::kType_MouseDown, 20, 65, ultralight::MouseEvent::kButton_Left});
    view.FireMouseEvent({ultralight::MouseEvent::kType_MouseUp, 20, 65, ultralight::MouseEvent::kButton_Left});
    check(view.HasInputFocus(), "Input did not receive focus");
    ultralight::KeyEvent key;
    key.type = ultralight::KeyEvent::kType_Char; key.modifiers = 0;
    key.virtual_key_code = 0; key.native_key_code = 0;
    key.text = "x"; key.unmodified_text = "x";
    key.is_keypad = key.is_auto_repeat = key.is_system_key = false;
    view.FireKeyEvent(key);
    check(number("i.value==='x'?1:0") == 1, "Char input failed");
    view.FireMouseEvent({ultralight::MouseEvent::kType_MouseMoved, 200, 150, ultralight::MouseEvent::kButton_None});
    view.FireScrollEvent({ultralight::ScrollEvent::kType_ScrollByPixel, 0, -100, 200, 150});
    pump(runtime, [&] { return number("scrollY") > 0; });
    view.WithBridge([](auto &bridge) { bridge.evaluateRetained("scrollTo(0,0)"); });
    pump(runtime, [&] { return number("scrollY") == 0; });
    runtime.Update(); runtime.RefreshDisplay(); runtime.Render();
    bool called = false;
    view.WithBridge([&](auto &bridge) {
        bridge.bindFunc("hit", [&](JSContextRef ctx, JSObjectRef, std::span<const JSValueRef>) {
            called = true;
            expect(ErrorCode::Busy, [&] { view.Focus(); });
            expect(ErrorCode::Busy, [&] { runtime.Update(); });
            if (close_view) view.Close();
            if (close_runtime) runtime.Close();
            return JSValueMakeUndefined(ctx);
        });
    });
    click(); check(called, "Bound input callback was not called");
    if (close_view || close_runtime) expect(ErrorCode::Closed, [&] { view.HasFocus(); });
    else { view.Focus(); runtime.Update(); view.Close(); expect(ErrorCode::Closed, [&] { view.Focus(); }); }
}

int main(int argc, char **argv) {
    return sample::run([&] {
        auto runtime = ulbind17::Runtime::Create();
        std::string mode = argc > 1 ? argv[1] : "dynamic";
        if (mode == "dynamic") dynamic(runtime);
        else if (mode == "lifecycle") lifecycle(runtime);
        else if (mode == "threading") threading(runtime);
        else if (mode == "input") input_events(runtime);
        else if (mode == "input-close") input_events(runtime, true);
        else if (mode == "input-runtime-close") input_events(runtime, false, true);
        else throw std::invalid_argument("Unknown runtime test");
        runtime.Close();
    });
}
