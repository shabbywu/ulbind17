#pragma once
#include <ulbind17/values.hpp>
#include <Ultralight/js/API.h>
#include <Ultralight/js/Task.h>
#include <string>
#include <utility>

namespace ulbind17 {

// Thin conveniences over SDK handles. Results retain SDK conversion, exception and
// page-gone errors; raw JavaScriptCore handles are deliberately not accepted here.
class Object {
    js::Value value_;
  public:
    explicit Object(js::Value value) : value_(std::move(value)) {}
    explicit Object(const js::Context &context) : value_(context.MakeObject()) {}
    static Object GetGlobalObject(const js::Context &context) { return Object(context.GlobalObject()); }
    const js::Value &value() const { return value_; }
    template <typename T> js::Result<T> get(std::string_view name) const { return ulbind17::get<T>(value_, name); }
    template <typename T> js::Result<void> set(const char *name, T &&value) const {
        js::Context context(value_);
        return value_.SetProperty(name, context.Make(std::forward<T>(value)));
    }
    // Object functions are synchronous and survive this Object handle. Capture
    // owning/weak host references; use Bindings for host lifetime and async work.
    template <typename F> js::Result<void> bindFunc(const char *name, F &&callback) const {
        js::Context context(value_);
        return value_.SetProperty(name, context.MakeFunction(name, std::forward<F>(callback)));
    }
    template <typename R = js::Value, typename... A>
    js::Result<R> call(const char *name, A &&...args) const {
        if constexpr (std::same_as<R, void>) {
            auto result = value_[name].template Invoke<js::Value>(std::forward<A>(args)...);
            if (!result) return js::Unexpected(std::move(result).error());
            return {};
        } else return value_[name].template Invoke<R>(std::forward<A>(args)...);
    }
    auto keys() const { return ulbind17::keys(value_); }
    auto size() const { return ulbind17::size(value_); }
};

template <typename Signature> class Function;
template <typename R, typename... A> class Function<R(A...)> {
    js::Value function_;
  public:
    explicit Function(js::Value value) : function_(std::move(value)) {}
    js::Result<R> operator()(A... args) const {
        if constexpr (std::same_as<R, void>) {
            auto result = function_.template Invoke<js::Value>(std::forward<A>(args)...);
            if (!result) return js::Unexpected(std::move(result).error());
            return {};
        } else return function_.template Invoke<R>(std::forward<A>(args)...);
    }
    const js::Value &value() const { return function_; }
};

class Script {
    js::Context context_;
    std::string source_;
  public:
    Script(js::Context context, std::string source) : context_(std::move(context)), source_(std::move(source)) {}
    template <typename R = js::Value> js::Result<R> Evaluate() const {
        if constexpr (std::same_as<R, js::Value>) return context_.Evaluate(source_);
        else if constexpr (std::same_as<R, void>) {
            auto result = context_.Evaluate(source_);
            if (!result) return js::Unexpected(std::move(result).error());
            return {};
        }
        else return context_.template Evaluate<R>(source_);
    }
};

// SDK API lifetime also cancels suspended Task bindings when its owner is gone.
// Bind before AttachTo; the SDK reinjects the namespace on subsequent navigation.
class Bindings {
    js::API api_;
  public:
    explicit Bindings(const char *name) : api_(name) {}
    template <typename F> void bindFunc(const char *name, F &&callback) {
        api_.Bind(name, std::forward<F>(callback));
    }
    bool AttachTo(ultralight::View *view, const js::AttachOptions &options = {}) { return api_.AttachTo(view, options); }
    js::API &api() { return api_; }
};

} // namespace ulbind17
