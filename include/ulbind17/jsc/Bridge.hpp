#pragma once
#include <JavaScriptCore/JavaScript.h>
#include <Ultralight/js/Context.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Logger.h>
#include <ulbind17/Error.hpp>
#include <ulbind17/jsc/Traversal.hpp>
#include <thread>
#include <cmath>
#include <limits>
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ulbind17::jsc {

class String {
    JSStringRef value_;
  public:
    explicit String(std::string_view value) {
        auto utf16 = ultralight::String(value.data(), value.size()).utf16();
        value_ = JSStringCreateWithCharacters(reinterpret_cast<const JSChar *>(utf16.data()), utf16.length());
    }
    explicit String(std::span<const char16_t> value)
        : value_(JSStringCreateWithCharacters(reinterpret_cast<const JSChar *>(value.data()), value.size())) {}
    ~String() { JSStringRelease(value_); }
    String(const String &) = delete;
    String &operator=(const String &) = delete;
    operator JSStringRef() const { return value_; }
};

inline std::string utf8(JSStringRef text) {
    if (!text) return {};
    std::string result(JSStringGetMaximumUTF8CStringSize(text), '\0');
    auto length = JSStringGetUTF8CString(text, result.data(), result.size());
    result.resize(length ? length - 1 : 0);
    return result;
}
inline std::string text(JSContextRef context, JSValueRef value) {
    if (!context || !value) throw std::invalid_argument("Empty JavaScript context or value");
    JSValueRef exception = nullptr;
    auto string = JSValueToStringCopy(context, value, &exception);
    if (exception || !string) {
        if (string) JSStringRelease(string);
        auto reason = exception ? JSValueToStringCopy(context, exception, nullptr) : nullptr;
        auto message = reason ? utf8(reason) : "JavaScript string conversion failed";
        if (reason) JSStringRelease(reason);
        throw std::runtime_error(message);
    }
    auto result = utf8(string);
    JSStringRelease(string);
    return result;
}
inline void check(JSContextRef context, JSValueRef exception) {
    if (!exception) return;
    Error error(ErrorCode::ScriptException, "JavaScript exception");
    JSValueProtect(context, exception);
    try {
        try { error = Error(ErrorCode::ScriptException, text(context, exception)); } catch (...) {}
        if (JSValueIsObject(context, exception)) {
            auto object = JSValueToObject(context, exception, nullptr);
            auto property = [&](const char *name) -> JSValueRef {
                String key(name); JSValueRef nested = nullptr;
                auto value = JSObjectGetProperty(context, object, key, &nested);
                return nested ? nullptr : value;
            };
            auto string = [&](const char *name) {
                auto value = property(name);
                return value && JSValueIsString(context, value) ? text(context, value) : std::string();
            };
            auto number = [&](const char *name) {
                auto value = property(name);
                return value && JSValueIsNumber(context, value) ? JSValueToNumber(context, value, nullptr) : 0.;
            };
            error.stack = string("stack"); error.source_url = string("sourceURL");
            error.line = number("line"); error.column = number("column");
        }
    } catch (...) { /* Diagnostics getters must not replace the original failure. */ }
    JSValueUnprotect(context, exception);
    throw error;
}
inline void set_exception(JSContextRef context, JSValueRef *exception, const char *message) {
    if (!exception) return;
    String string(message);
    auto value = JSValueMakeString(context, string);
    *exception = JSObjectMakeError(context, 1, &value, nullptr);
}
inline JSGlobalContextRef retain_context(JSContextRef context) {
    if (!context) throw std::invalid_argument("Empty JavaScript context");
    return JSGlobalContextRetain(JSContextGetGlobalContext(context));
}

class ContextScope {
    JSGlobalContextRef context_;
  public:
    explicit ContextScope(JSContextRef context)
        : context_(retain_context(context)) {}
    ~ContextScope() { JSGlobalContextRelease(context_); }
    ContextScope(const ContextScope &) = delete;
    ContextScope &operator=(const ContextScope &) = delete;
};
class Root {
    JSGlobalContextRef context_;
    JSValueRef value_;
  public:
    Root(JSContextRef context, JSValueRef value)
        : context_(retain_context(context)), value_(value) {
        if (!value_) {
            JSGlobalContextRelease(context_);
            throw std::invalid_argument("Cannot protect an empty JavaScript value");
        }
        JSValueProtect(context_, value_);
    }
    ~Root() { JSValueUnprotect(context_, value_); JSGlobalContextRelease(context_); }
    Root(const Root &) = delete;
    Root &operator=(const Root &) = delete;
};

// A document-scoped scaffold for embedders with dynamic host types. All JSC
// operations run on the renderer thread. Use SDK Context::PostTask or Resolver
// for worker completion; never send raw JSC values to a worker thread.
class Bridge : public std::enable_shared_from_this<Bridge> {
  public:
    // A host may release a Handle on another thread (eg, a managed finalizer).
    // Unprotect is posted to the renderer; navigation releases roots immediately.
    class Handle {
        std::weak_ptr<Bridge> bridge_;
        ultralight::js::Context document_;
        JSValueRef value_;
        friend class Bridge;
        Handle(std::weak_ptr<Bridge> bridge, ultralight::js::Context document, JSValueRef value)
            : bridge_(std::move(bridge)), document_(std::move(document)), value_(value) {}
      public:
        ~Handle();
        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        JSValueRef value() const;
    };
    using Callback = std::function<JSValueRef(JSContextRef, JSObjectRef, std::span<const JSValueRef>)>;
    using Getter = std::function<JSValueRef(JSContextRef, JSStringRef)>;
    using Setter = std::function<bool(JSContextRef, JSStringRef, JSValueRef)>;
    using HasProperty = std::function<bool(JSContextRef, JSStringRef)>;
    using Enumerator = std::function<std::vector<std::string>(JSContextRef)>;
    struct ObjectHooks { Getter get; Setter set; HasProperty has; Enumerator keys; };
    using Settlement = std::function<void(JSValueRef, bool)>;
  private:
    struct Handlers {
        Callback call;
        Getter get;
        Setter set;
        HasProperty has;
        Enumerator keys;
        void clear() { call = {}; get = {}; set = {}; has = {}; keys = {}; }
    };
    struct CallbackData { std::weak_ptr<Bridge> bridge; std::shared_ptr<Handlers> handlers; };
    template <typename T> struct ProxyData : CallbackData { T payload; };
    struct Observer {
        Settlement settled;
        std::function<void()> cancelled;
        void finish(JSValueRef value, bool rejected) {
            auto callback = std::move(settled);
            cancelled = {};
            if (callback) callback(value, rejected);
        }
        void cancel() {
            settled = {};
            auto callback = std::move(cancelled);
            if (callback) callback();
        }
    };
    JSGlobalContextRef context_;
    const std::thread::id thread_ = std::this_thread::get_id();
    ultralight::js::Context document_;
    std::vector<std::weak_ptr<Handlers>> handlers_;
    std::vector<std::weak_ptr<Observer>> observers_;
    struct Protected { std::unique_ptr<Root> root; size_t handles = 0; bool permanent = false; };
    std::map<JSValueRef, Protected> roots_;

    Protected &root(JSValueRef value) {
        auto context = this->context();
        auto found = roots_.find(value);
        if (found == roots_.end()) found = roots_.emplace(value, Protected{std::make_unique<Root>(context, value)}).first;
        return found->second;
    }
    void release(JSValueRef value) {
        auto found = roots_.find(value);
        if (found != roots_.end() && found->second.handles && --found->second.handles == 0 && !found->second.permanent)
            roots_.erase(found);
    }

    explicit Bridge(JSContextRef context, ultralight::View *view)
        : context_(retain_context(context)), document_(view) {}
    static JSClassRef callback_class() {
        static auto type = [] {
            auto definition = kJSClassDefinitionEmpty;
            definition.className = "UlbindCallback";
            definition.callAsFunction = [](JSContextRef context, JSObjectRef function, JSObjectRef receiver,
                                           size_t count, const JSValueRef *args, JSValueRef *exception) -> JSValueRef {
                auto *data = static_cast<CallbackData *>(JSObjectGetPrivate(function));
                try {
                    auto bridge = data->bridge.lock();
                    if (!bridge || !bridge->active() || !data->handlers->call)
                        throw std::runtime_error("Native callback belongs to an expired document");
                    ContextScope scope(context);
                    auto callback = data->handlers->call;
                    auto result = callback(context, receiver, {args, count});
                    return bridge->active() && result ? result : JSValueMakeUndefined(context);
                } catch (const std::exception &error) { set_exception(context, exception, error.what()); }
                catch (...) { set_exception(context, exception, "Native callback threw an unknown exception"); }
                return JSValueMakeUndefined(context);
            };
            definition.finalize = [](JSObjectRef object) { delete static_cast<CallbackData *>(JSObjectGetPrivate(object)); };
            return JSClassCreate(&definition);
        }();
        return type;
    }
    template <typename T, bool Extended = false> static JSClassRef proxy_class() {
        static auto type = [] {
            auto definition = kJSClassDefinitionEmpty;
            definition.className = "UlbindObject";
            definition.getProperty = [](JSContextRef context, JSObjectRef object, JSStringRef name,
                                        JSValueRef *exception) -> JSValueRef {
                auto *data = static_cast<ProxyData<T> *>(JSObjectGetPrivate(object));
                try {
                    auto bridge = data->bridge.lock();
                    if (!bridge || !bridge->active()) throw std::runtime_error("Native object belongs to an expired document");
                    auto getter = data->handlers->get;
                    if (!getter) return nullptr;
                    ContextScope scope(context);
                    auto result = getter(context, name);
                    return bridge->active() ? result : JSValueMakeUndefined(context);
                } catch (const std::exception &error) { set_exception(context, exception, error.what()); }
                catch (...) { set_exception(context, exception, "Native getter threw an unknown exception"); }
                return JSValueMakeUndefined(context);
            };
            definition.setProperty = [](JSContextRef context, JSObjectRef object, JSStringRef name,
                                        JSValueRef value, JSValueRef *exception) -> bool {
                auto *data = static_cast<ProxyData<T> *>(JSObjectGetPrivate(object));
                try {
                    auto bridge = data->bridge.lock();
                    if (!bridge || !bridge->active()) throw std::runtime_error("Native object belongs to an expired document");
                    auto setter = data->handlers->set;
                    ContextScope scope(context);
                    return setter ? setter(context, name, value) : false;
                } catch (const std::exception &error) { set_exception(context, exception, error.what()); }
                catch (...) { set_exception(context, exception, "Native setter threw an unknown exception"); }
                return true;
            };
            // JSC supplies no exception output for these two hooks. Contain failures
            // and log them rather than unwinding through its C boundary.
            if constexpr (Extended) definition.hasProperty = [](JSContextRef context, JSObjectRef object, JSStringRef name) -> bool {
                auto *data = static_cast<ProxyData<T> *>(JSObjectGetPrivate(object));
                try {
                    auto bridge = data->bridge.lock();
                    if (!bridge || !bridge->active()) return false;
                    auto has = data->handlers->has;
                    ContextScope scope(context);
                    if (has) { auto found = has(context, name); return bridge->active() && found; }
                    auto enumerate = data->handlers->keys;
                    if (!enumerate) return false;
                    auto names = enumerate(context);
                    return bridge->active() && std::find(names.begin(), names.end(), utf8(name)) != names.end();
                } catch (const std::exception &error) { report_hook_error(error.what()); }
                catch (...) { report_hook_error("Native property existence hook threw"); }
                return false;
            };
            if constexpr (Extended) definition.getPropertyNames = [](JSContextRef context, JSObjectRef object,
                                              JSPropertyNameAccumulatorRef names) {
                auto *data = static_cast<ProxyData<T> *>(JSObjectGetPrivate(object));
                try {
                    auto bridge = data->bridge.lock();
                    if (!bridge || !bridge->active()) return;
                    auto enumerate = data->handlers->keys;
                    if (!enumerate) return;
                    ContextScope scope(context);
                    auto keys = enumerate(context);
                    if (!bridge->active()) return;
                    for (const auto &key : keys) { String name(key); JSPropertyNameAccumulatorAddName(names, name); }
                } catch (const std::exception &error) { report_hook_error(error.what()); }
                catch (...) { report_hook_error("Native enumeration hook threw"); }
            };
            definition.finalize = [](JSObjectRef object) { delete static_cast<ProxyData<T> *>(JSObjectGetPrivate(object)); };
            return JSClassCreate(&definition);
        }();
        return type;
    }
    static void report_hook_error(const char *message) noexcept {
        try {
            auto logger = ultralight::Platform::instance().logger();
            if (logger) logger->LogMessage(ultralight::LogLevel::Error, message);
        } catch (...) {}
    }
    JSObjectRef object(JSValueRef value) const {
        auto ctx = context();
        if (!value || !JSValueIsObject(ctx, value))
            throw Error(ErrorCode::TypeMismatch, "Expected a JavaScript object");
        return JSValueToObject(ctx, value, nullptr);
    }
    void ensure_alive() const {
        if (!active()) throw Error(ErrorCode::PageGone, "JavaScript document expired during operation");
    }
    JSValueRef checked(const Handle &handle) const {
        if (handle.bridge_.lock().get() != this)
            throw Error(ErrorCode::CrossContext, "JavaScript handle belongs to another bridge");
        return handle.value();
    }
    std::shared_ptr<Handlers> track() {
        std::erase_if(handlers_, [](const auto &weak) { return weak.expired(); });
        auto handlers = std::make_shared<Handlers>();
        handlers_.push_back(handlers);
        return handlers;
    }
  public:
    static std::shared_ptr<Bridge> create(JSContextRef context, ultralight::View *view) {
        if (!context || !view) throw std::invalid_argument("A bridge requires a live View and context");
        auto bridge = std::shared_ptr<Bridge>(new Bridge(context, view));
        if (ulJSContextGetJSContextRef(bridge->document_.raw()) != context)
            throw std::invalid_argument("The context does not belong to the View's main document");
        return bridge;
    }
    ~Bridge() { invalidate(); }
    bool active() const { return context_ && document_.IsAlive(); }
    bool matches(JSContextRef context) const { return active() && context_ == JSContextGetGlobalContext(context); }
    JSGlobalContextRef context() const {
        if (std::this_thread::get_id() != thread_)
            throw Error(ErrorCode::WrongThread, "JavaScript operations require the renderer thread");
        if (!active()) throw std::runtime_error("JavaScript document is no longer valid");
        return context_;
    }
    const ultralight::js::Context &document() const { return document_; }
    void invalidate() noexcept {
        if (!context_) return;
        auto context = std::exchange(context_, nullptr);
        // Mark inactive before releasing host closures or notifying observers.
        auto observers = std::move(observers_);
        for (auto &weak : observers) if (auto observer = weak.lock()) {
            // A host cancellation callback must not prevent cleanup of other handles.
            try { observer->cancel(); } catch (...) {}
        }
        for (auto &weak : handlers_) if (auto handlers = weak.lock()) handlers->clear();
        handlers_.clear(); roots_.clear();
        document_ = {};
        JSGlobalContextRelease(context);
    }
    void protect(JSValueRef value) {
        root(value).permanent = true;
    }
    std::unique_ptr<Handle> retain(JSValueRef value) {
        auto handle = std::unique_ptr<Handle>(new Handle(weak_from_this(), document_, value));
        ++root(value).handles;
        return handle;
    }
    JSObjectRef makeFunction(Callback callback) {
        auto context = this->context();
        auto handlers = track(); handlers->call = std::move(callback);
        return JSObjectMake(context, callback_class(), new CallbackData{weak_from_this(), handlers});
    }
    template <typename T> JSObjectRef makeObject(T payload, Getter get, Setter set = {}) {
        return makeObject(std::move(payload), ObjectHooks{std::move(get), std::move(set), {}, {}});
    }
    template <typename T> JSObjectRef makeObject(T payload, ObjectHooks hooks) {
        auto context = this->context();
        auto handlers = track(); handlers->get = std::move(hooks.get); handlers->set = std::move(hooks.set);
        handlers->has = std::move(hooks.has); handlers->keys = std::move(hooks.keys);
        auto type = handlers->has || handlers->keys ? proxy_class<T, true>() : proxy_class<T>();
        return JSObjectMake(context, type, new ProxyData<T>{{weak_from_this(), handlers}, std::move(payload)});
    }
    template <typename T> const T *unwrap(JSValueRef value) const {
        auto context = this->context();
        if (!JSValueIsObjectOfClass(context, value, proxy_class<T>()) &&
            !JSValueIsObjectOfClass(context, value, proxy_class<T, true>())) return nullptr;
        auto *data = static_cast<ProxyData<T> *>(JSObjectGetPrivate(JSValueToObject(context, value, nullptr)));
        return data->bridge.lock().get() == this ? &data->payload : nullptr;
    }
    void bind(JSStringRef name, JSValueRef value) {
        auto context = this->context(); Root root(context, value);
        JSValueRef exception = nullptr;
        JSObjectSetProperty(context, JSContextGetGlobalObject(context), name, value, kJSPropertyAttributeNone, &exception);
        check(context, exception);
    }
    void bindFunc(const char *name, Callback callback) {
        String key(name); bind(key, makeFunction(std::move(callback)));
    }
    JSValueRef evaluate(JSStringRef script) {
        auto context = this->context(); ContextScope scope(context);
        JSValueRef exception = nullptr;
        auto value = JSEvaluateScript(context, script, nullptr, nullptr, 1, &exception);
        if (!active()) return nullptr;
        check(context, exception);
        return value;
    }
    JSValueRef call(JSObjectRef function, std::span<const JSValueRef> args = {}, JSObjectRef receiver = nullptr) {
        auto context = this->context(); ContextScope scope(context); Root root(context, function);
        JSValueRef exception = nullptr;
        auto value = JSObjectCallAsFunction(context, function, receiver, args.size(), args.data(), &exception);
        if (!active()) return nullptr;
        check(context, exception);
        return value;
    }
    JSValueRef invoke(const char *name, std::span<const JSValueRef> args = {}) {
        auto context = this->context(); ContextScope scope(context); String key(name);
        JSValueRef exception = nullptr;
        auto value = JSObjectGetProperty(context, JSContextGetGlobalObject(context), key, &exception);
        if (!active()) return nullptr;
        check(context, exception);
        if (!JSValueIsObject(context, value) || !JSObjectIsFunction(context, JSValueToObject(context, value, nullptr)))
            throw std::runtime_error(std::string("JavaScript function not found: ") + name);
        return call(JSValueToObject(context, value, nullptr), args, JSContextGetGlobalObject(context));
    }
    // Raw values are borrowed, belong to this document, and require a Root when
    // retained across allocating JSC calls. Retained variants below own their roots.
    JSValueRef get(JSValueRef value, std::string_view name) {
        auto ctx = context(); ContextScope scope(ctx); Root root(ctx, value); String key(name);
        auto target = object(value); JSValueRef exception = nullptr;
        auto result = JSObjectGetProperty(ctx, target, key, &exception);
        ensure_alive(); check(ctx, exception); return result;
    }
    void set(JSValueRef value, std::string_view name, JSValueRef item) {
        auto ctx = context(); ContextScope scope(ctx); Root target_root(ctx, value), item_root(ctx, item); String key(name);
        JSValueRef exception = nullptr;
        JSObjectSetProperty(ctx, object(value), key, item, kJSPropertyAttributeNone, &exception);
        ensure_alive(); check(ctx, exception);
    }
    bool has(JSValueRef value, std::string_view name) {
        auto ctx = context(); ContextScope scope(ctx); Root target_root(ctx, value);
        object(value);
        auto reflect = get(JSContextGetGlobalObject(ctx), "Reflect"); Root reflect_root(ctx, reflect);
        auto function = get(reflect, "has"); Root function_root(ctx, function);
        String key(name); JSValueRef args[] = {value, JSValueMakeString(ctx, key)};
        auto result = call(object(function), args, object(reflect)); ensure_alive();
        return readBoolean(result);
    }
    JSValueRef at(JSValueRef value, uint32_t index) { return get(value, std::to_string(index)); }
    void setAt(JSValueRef value, uint32_t index, JSValueRef item) { set(value, std::to_string(index), item); }
    std::size_t length(JSValueRef value) {
        auto ctx = context();
        if (!value || !JSValueIsArray(ctx, value)) throw Error(ErrorCode::TypeMismatch, "Expected a JavaScript array");
        auto size = readNumber(get(value, "length"));
        if (!std::isfinite(size) || size < 0 || size > (std::numeric_limits<uint32_t>::max)() || std::floor(size) != size)
            throw Error(ErrorCode::TypeMismatch, "Invalid JavaScript array length");
        return static_cast<std::size_t>(size);
    }
    std::vector<std::string> keys(JSValueRef value) {
        auto ctx = context(); ContextScope scope(ctx); Root target_root(ctx, value); object(value);
        auto builtin = get(JSContextGetGlobalObject(ctx), "Object"); Root builtin_root(ctx, builtin);
        auto function = get(builtin, "keys"); Root function_root(ctx, function);
        JSValueRef args[] = {value}; auto names = call(object(function), args, object(builtin));
        ensure_alive(); Root names_root(ctx, names);
        std::vector<std::string> result;
        auto size = length(names); result.reserve(size);
        for (std::size_t i = 0; i < size; ++i) result.push_back(readString(at(names, static_cast<uint32_t>(i))));
        return result;
    }
    JSValueRef makeBoolean(bool value) { return JSValueMakeBoolean(context(), value); }
    JSValueRef makeNumber(double value) { return JSValueMakeNumber(context(), value); }
    JSValueRef makeString(std::string_view value) {
        auto ctx = context(); String text(value); return JSValueMakeString(ctx, text);
    }
    JSValueRef makeNull() { return JSValueMakeNull(context()); }
    JSValueRef makeUndefined() { return JSValueMakeUndefined(context()); }
    JSObjectRef makePlainObject() { return JSObjectMake(context(), nullptr, nullptr); }
    JSObjectRef makeArray(std::span<const JSValueRef> values = {}) {
        auto ctx = context(); ContextScope scope(ctx);
        std::vector<std::unique_ptr<Root>> roots;
        for (auto value : values) roots.push_back(std::make_unique<Root>(ctx, value));
        JSValueRef exception = nullptr; auto array = JSObjectMakeArray(ctx, values.size(), values.data(), &exception);
        ensure_alive(); check(ctx, exception); return array;
    }
    bool readBoolean(JSValueRef value) const {
        auto ctx = context();
        if (!value || !JSValueIsBoolean(ctx, value)) throw Error(ErrorCode::TypeMismatch, "Expected a JavaScript boolean");
        return JSValueToBoolean(ctx, value);
    }
    double readNumber(JSValueRef value) const {
        auto ctx = context();
        if (!value || !JSValueIsNumber(ctx, value)) throw Error(ErrorCode::TypeMismatch, "Expected a JavaScript number");
        return JSValueToNumber(ctx, value, nullptr);
    }
    std::string readString(JSValueRef value) const {
        auto ctx = context();
        if (!value || !JSValueIsString(ctx, value)) throw Error(ErrorCode::TypeMismatch, "Expected a JavaScript string");
        return text(ctx, value);
    }
    std::unique_ptr<Handle> evaluateRetained(std::string_view source) {
        context(); // Reject worker access before constructing a JSC string.
        String script(source); auto result = evaluate(script); ensure_alive(); return retain(result);
    }
    std::unique_ptr<Handle> getRetained(const Handle &target, std::string_view name) {
        return retain(get(checked(target), name));
    }
    std::unique_ptr<Handle> callRetained(const Handle &function,
        std::span<const Handle *const> args = {}, const Handle *receiver = nullptr) {
        auto ctx = context(); std::vector<JSValueRef> values;
        for (auto arg : args) {
            if (!arg) throw Error(ErrorCode::InvalidArgument, "Empty argument handle");
            values.push_back(checked(*arg));
        }
        auto fn = object(checked(function));
        if (!JSObjectIsFunction(ctx, fn)) throw Error(ErrorCode::TypeMismatch, "Expected a JavaScript function");
        auto result = call(fn, values, receiver ? object(checked(*receiver)) : nullptr);
        ensure_alive(); return retain(result);
    }
    bool isPromise(JSValueRef value) const {
        if (!JSValueIsObject(context(), value)) return false;
        JSPromiseStatus status;
        return JSPromiseGetStatus(context(), JSValueToObject(context(), value, nullptr), &status);
    }
    void then(JSValueRef promise, Settlement settled, std::function<void()> cancelled = {}) {
        auto context = this->context(); Root root(context, promise);
        if (!isPromise(promise)) throw std::invalid_argument("then() requires a JavaScript Promise");
        auto observer = std::make_shared<Observer>(Observer{std::move(settled), std::move(cancelled)});
        std::erase_if(observers_, [](const auto &weak) { return weak.expired(); });
        observers_.push_back(observer);
        auto fulfilled = makeFunction([observer](JSContextRef context, JSObjectRef, std::span<const JSValueRef> args) {
            observer->finish(args.empty() ? JSValueMakeUndefined(context) : args[0], false);
            return JSValueMakeUndefined(context);
        });
        Root fulfilled_root(context, fulfilled);
        auto rejected = makeFunction([observer](JSContextRef context, JSObjectRef, std::span<const JSValueRef> args) {
            observer->finish(args.empty() ? JSValueMakeUndefined(context) : args[0], true);
            return JSValueMakeUndefined(context);
        });
        Root rejected_root(context, rejected);
        JSValueRef exception = nullptr; String name("then");
        auto object = JSValueToObject(context, promise, nullptr);
        auto method = JSObjectGetProperty(context, object, name, &exception); check(context, exception);
        if (!method || !JSValueIsObject(context, method)) throw std::runtime_error("Promise.then is not callable");
        JSValueRef args[] = {fulfilled, rejected};
        call(JSValueToObject(context, method, nullptr), args, object);
    }
};

inline Bridge::Handle::~Handle() {
    document_.PostTask([weak = bridge_, value = value_] {
        if (auto bridge = weak.lock(); bridge && bridge->active()) bridge->release(value);
    });
}
inline JSValueRef Bridge::Handle::value() const {
    auto bridge = bridge_.lock();
    if (bridge && std::this_thread::get_id() != bridge->thread_)
        throw Error(ErrorCode::WrongThread, "JavaScript handle access requires the renderer thread");
    if (!bridge || !bridge->active())
        throw Error(ErrorCode::PageGone, "JavaScript handle belongs to an expired document");
    return value_;
}

} // namespace ulbind17::jsc
