#pragma once
#include <JavaScriptCore/JavaScript.h>
#include <Ultralight/js/Context.h>
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
    if (exception) throw std::runtime_error(text(context, exception));
}
inline void set_exception(JSContextRef context, JSValueRef *exception, const char *message) {
    if (!exception) return;
    String string(message);
    auto value = JSValueMakeString(context, string);
    *exception = JSObjectMakeError(context, 1, &value, nullptr);
}

class ContextScope {
    JSGlobalContextRef context_;
  public:
    explicit ContextScope(JSContextRef context)
        : context_(JSGlobalContextRetain(JSContextGetGlobalContext(context))) {}
    ~ContextScope() { JSGlobalContextRelease(context_); }
    ContextScope(const ContextScope &) = delete;
    ContextScope &operator=(const ContextScope &) = delete;
};
class Root {
    JSGlobalContextRef context_;
    JSValueRef value_;
  public:
    Root(JSContextRef context, JSValueRef value)
        : context_(JSGlobalContextRetain(JSContextGetGlobalContext(context))), value_(value) {
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
    using Callback = std::function<JSValueRef(JSContextRef, JSObjectRef, std::span<const JSValueRef>)>;
    using Getter = std::function<JSValueRef(JSContextRef, JSStringRef)>;
    using Setter = std::function<bool(JSContextRef, JSStringRef, JSValueRef)>;
    using Settlement = std::function<void(JSValueRef, bool)>;
  private:
    struct Handlers {
        Callback call;
        Getter get;
        Setter set;
        void clear() { call = {}; get = {}; set = {}; }
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
    ultralight::js::Context document_;
    std::vector<std::weak_ptr<Handlers>> handlers_;
    std::vector<std::weak_ptr<Observer>> observers_;
    std::map<JSValueRef, std::unique_ptr<Root>> roots_;

    explicit Bridge(JSContextRef context, ultralight::View *view)
        : context_(JSGlobalContextRetain(JSContextGetGlobalContext(context))), document_(view) {}
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
    template <typename T> static JSClassRef proxy_class() {
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
            definition.finalize = [](JSObjectRef object) { delete static_cast<ProxyData<T> *>(JSObjectGetPrivate(object)); };
            return JSClassCreate(&definition);
        }();
        return type;
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
        if (!roots_.contains(value)) roots_.emplace(value, std::make_unique<Root>(context(), value));
    }
    JSObjectRef makeFunction(Callback callback) {
        auto context = this->context();
        auto handlers = track(); handlers->call = std::move(callback);
        return JSObjectMake(context, callback_class(), new CallbackData{weak_from_this(), handlers});
    }
    template <typename T> JSObjectRef makeObject(T payload, Getter get, Setter set = {}) {
        auto context = this->context();
        auto handlers = track(); handlers->get = std::move(get); handlers->set = std::move(set);
        return JSObjectMake(context, proxy_class<T>(), new ProxyData<T>{{weak_from_this(), handlers}, std::move(payload)});
    }
    template <typename T> const T *unwrap(JSValueRef value) const {
        auto context = this->context();
        if (!JSValueIsObjectOfClass(context, value, proxy_class<T>())) return nullptr;
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

} // namespace ulbind17::jsc
