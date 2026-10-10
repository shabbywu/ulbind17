#include "../samples/common.hpp"
#include <ulbind17/jsc/Bridge.hpp>
#include <optional>
#include <thread>

namespace js = ulbind17::js;
using sample::check;
using sample::take;

template <typename F> void pump(sample::Fixture &fixture, F ready) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready() && std::chrono::steady_clock::now() < deadline) {
        fixture.renderer->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(ready(), "asynchronous operation timed out");
}

void conveniences() {
    sample::Fixture fixture;
    js::Context context(fixture.view.get());
    auto global = ulbind17::Object::GetGlobalObject(context);
    take(global.bindFunc("hello", [](std::string name) { return "你好 " + name; }));
    check(take(global.call<std::string>("hello", "🐉")) == "你好 🐉", "global binding failed");
    ulbind17::Object object(context);
    take(object.set("number", 7));
    check(take(object.get<int>("number")) == 7 && take(object.size()) == 1, "object facade failed");
    take(object.bindFunc("add", [](int a, int b) { return a + b; }));
    auto wrong = object.call<int>("add", "bad", 2);
    check(!wrong && wrong.error().code() == "ULJS_BAD_ARG", "callback conversion error was lost");
    take(global.set("box", object.value()));
    ulbind17::Script script(context, "box.add(3,4)");
    check(take(script.Evaluate<int>()) == 7, "script facade failed");
    take(script.Evaluate<void>());
    ulbind17::Function<int(int)> function(take(context.Evaluate("x => x * 2")));
    check(take(function(4)) == 8, "function facade failed");
    ulbind17::Function<void()> void_function(take(context.Evaluate("()=>{globalThis.voidCalled=true}")));
    take(void_function());
    take(global.call<void>("hello", "void result"));
    check(take(context.Evaluate<bool>("voidCalled")), "void function facade failed");
    auto thrown = ulbind17::Script(context, "throw new Error('checked failure')").Evaluate();
    check(!thrown && thrown.error().message().find("checked failure") != std::string::npos, "script exception was lost");
    take(object.set("method", take(context.Evaluate("(function(){return this.number})"))));
    check(take(object.call<int>("method")) == 7, "method receiver was lost");
}

void scaffold() {
    sample::Fixture fixture;
    auto lock = fixture.view->LockJSContext();
    auto bridge = ulbind17::jsc::Bridge::create(lock->ctx(), fixture.view.get());
    auto context = bridge->context();
    std::string with_nul("a\0b", 3);
    ulbind17::jsc::String nul_string(with_nul);
    check(ulbind17::jsc::text(context, JSValueMakeString(context, nul_string)) == with_nul,
          "string facade truncated an embedded NUL");
    bridge->bindFunc("echo", [](JSContextRef, JSObjectRef, std::span<const JSValueRef> args) { return args[0]; });
    ulbind17::jsc::String source("echo('中文 🐉')");
    check(ulbind17::jsc::text(context, bridge->evaluate(source)) == "中文 🐉", "JSC scaffold binding failed");
    int number = 0;
    auto proxy = bridge->makeObject(42,
        [&number](JSContextRef context, JSStringRef) { return JSValueMakeNumber(context, number); },
        [&number](JSContextRef context, JSStringRef, JSValueRef value) {
            number = static_cast<int>(JSValueToNumber(context, value, nullptr)); return true;
        });
    check(bridge->unwrap<int>(proxy) && *bridge->unwrap<int>(proxy) == 42, "native proxy identity was lost");
    ulbind17::jsc::String name("native"); bridge->bind(name, proxy);
    ulbind17::jsc::String assign("native.value = 12; native.value");
    check(JSValueToNumber(context, bridge->evaluate(assign), nullptr) == 12 && number == 12, "proxy getter/setter failed");
    bridge->bindFunc("throwing", [](JSContextRef, JSObjectRef, std::span<const JSValueRef>) -> JSValueRef {
        throw std::runtime_error("native failure");
    });
    ulbind17::jsc::String caught("try{throwing();false}catch(e){e.message === 'native failure'}");
    check(JSValueToBoolean(context, bridge->evaluate(caught)), "native exception did not reach JS");
    ulbind17::jsc::String bad_text("({toString(){throw new Error('string failure')}})");
    try {
        ulbind17::jsc::text(context, bridge->evaluate(bad_text));
        throw std::logic_error("throwing string conversion was ignored");
    } catch (const std::runtime_error &error) {
        check(std::string(error.what()).find("string failure") != std::string::npos, "string exception was lost");
    }
    bool settled = false;
    ulbind17::jsc::String promise("(async()=>await Promise.resolve('awaited'))()");
    bridge->then(bridge->evaluate(promise), [&](JSValueRef value, bool rejected) {
        check(!rejected && ulbind17::jsc::text(context, value) == "awaited", "raw Promise settlement failed");
        settled = true;
    });
    pump(fixture, [&] { return settled; });
    bridge->invalidate();
    js::Context sdk(fixture.view.get());
    check(take(sdk.Evaluate<bool>("try{echo(1);false}catch(e){/expired/.test(e.message)}")), "saved callback entered expired bridge");
    auto reentrant = ulbind17::jsc::Bridge::create(context, fixture.view.get());
    reentrant->bindFunc("invalidateDuringCall", [weak = std::weak_ptr(reentrant)](
        JSContextRef context, JSObjectRef, std::span<const JSValueRef>) {
        weak.lock()->invalidate(); return JSValueMakeNumber(context, 42);
    });
    ulbind17::jsc::String invalidate("invalidateDuringCall()");
    check(reentrant->evaluate(invalidate) == nullptr, "invalidated evaluation returned an expired value");
}

js::Task<int> work(int number) {
    int result = co_await js::RunOnWorker([number] { return number * 2; });
    co_return result;
}
js::Task<int> await_page(js::Value function) {
    auto result = co_await js::Await<int>(function());
    if (!result) co_return std::move(result).error();
    co_return result.value() + 1;
}
js::Task<int> fail() { co_return js::Error::Make(js::ErrorType::Error, "async failure"); }

void async() {
    sample::Fixture fixture;
    ulbind17::Bindings bindings("native");
    bindings.bindFunc("work", &work);
    bindings.bindFunc("awaitPage", &await_page);
    bindings.bindFunc("fail", &fail);
    std::optional<js::Resolver> deferred;
    bindings.bindFunc("deferred", [&deferred](js::Resolver resolver) { deferred.emplace(std::move(resolver)); });
    check(bindings.AttachTo(fixture.view.get()), "async API attachment failed");
    js::Context context(fixture.view.get());
    auto promise = take(context.Evaluate(R"(
        (async()=>{
            const worked = await native.work(21);
            const nested = await native.awaitPage(async()=>await Promise.resolve(7));
            const later = await native.deferred();
            let rejection = '';
            try { await native.fail(); } catch(e) { rejection = e.message; }
            return [worked,nested,later,rejection];
        })()
    )"));
    pump(fixture, [&] { return deferred.has_value(); });
    std::thread worker([resolver = std::move(*deferred)] { resolver.Resolve(9); });
    worker.join(); deferred.reset();
    bool settled = false;
    check(promise.Then([&](js::Result<js::Value> result) {
        auto value = take(std::move(result));
        check(take(ulbind17::get<int>(value, 0)) == 42 && take(ulbind17::get<int>(value, 1)) == 8 &&
              take(ulbind17::get<int>(value, 2)) == 9 && take(ulbind17::get<std::string>(value, 3)) == "async failure",
              "async/await result or rejection was lost");
        settled = true;
    }), "Promise observer failed");
    pump(fixture, [&] { return settled; });
    bool rejected = false;
    auto rejected_promise = take(context.Evaluate("Promise.reject(new Error('rejected'))"));
    check(rejected_promise.Then<int>([&](js::Result<int> result) {
        check(!result && result.error().message().find("rejected") != std::string::npos, "Promise rejection was lost");
        rejected = true;
    }), "rejection observer failed");
    pump(fixture, [&] { return rejected; });
}

js::Task<int> suspended(js::Value function, int *resumed) {
    auto result = co_await js::Await<int>(function());
    ++*resumed;
    if (!result) co_return std::move(result).error();
    co_return result.value();
}

void expiry() {
    sample::Fixture fixture;
    js::Context context(fixture.view.get());
    int resumed = 0;
    auto bindings = std::make_unique<ulbind17::Bindings>("native");
    bindings->bindFunc("suspended", [&resumed](js::Value function) { return suspended(function, &resumed); });
    check(bindings->AttachTo(fixture.view.get()), "expiry API attachment failed");
    auto pending = take(context.Evaluate("native.suspended(()=>new Promise(r=>globalThis.resume=r))"));
    bool rejected = false;
    check(pending.Then<int>([&](js::Result<int> result) {
        check(!result && result.error().code() == "ULJS_DETACHED", "API teardown did not reject suspended task");
        rejected = true;
    }), "teardown observer failed");
    bindings.reset();
    take(context.Evaluate("resume(123)"));
    pump(fixture, [&] { return rejected; });
    check(resumed == 0, "cancelled coroutine resumed into destroyed host state");
    auto old_global = ulbind17::Object::GetGlobalObject(context);
    auto lock = fixture.view->LockJSContext();
    auto bridge = ulbind17::jsc::Bridge::create(lock->ctx(), fixture.view.get());
    bool cancelled = false;
    ulbind17::jsc::String script("new Promise(()=>{})");
    bridge->then(bridge->evaluate(script), [](JSValueRef, bool) { throw std::runtime_error("expired promise settled"); },
                 [&] { cancelled = true; });
    lock = nullptr;
    fixture.view->LoadHTML("<body>new document</body>");
    pump(fixture, [&] { return !context.IsAlive(); });
    check(!bridge->active(), "JSC bridge did not detect real navigation");
    auto stale = old_global.get<int>("anything");
    check(!stale && stale.error().is_page_gone(), "facade lost page-gone error");
    bridge->invalidate();
    check(cancelled, "pending raw Promise did not notify cancellation");
}

int main(int argc, char **argv) {
    std::string mode = argc > 1 ? argv[1] : "conveniences";
    return sample::run([&] {
        if (mode == "conveniences") conveniences();
        else if (mode == "scaffold") scaffold();
        else if (mode == "async") async();
        else if (mode == "expiry") expiry();
        else throw std::invalid_argument("unknown javascript test case");
    });
}
