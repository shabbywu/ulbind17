#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "ulbind17/detail/function/cpp_function.hpp"
#include "ulbind17/detail/function/js_call.hpp"
#include "ulbind17/detail/malloc.hpp"

namespace ulbind17 {
namespace detail {
class NativeFunctionBase {
  protected:
    static JSClassDefinition make_class_def(std::shared_ptr<generic_function> func) {
        JSClassDefinition def;
        memset(&def, 0, sizeof(def));
        def.className = "NativeFunction";
        def.attributes = kJSClassAttributeNone;
        def.callAsFunction = func->get_static_caller();
        def.finalize = &FinalizePrivateDataHolder;

        return def;
    }
    static JSClassRef make_class(std::shared_ptr<generic_function> func) {
        // dynamic data is store at class instance.
        // no matter what func it is, those classes are all same.
        static JSClassRef clazz;
        if (!clazz) {
            JSClassDefinition def = make_class_def(func);
            clazz = JSClassCreate(&def);
        }
        return clazz;
    }

    static JSObjectRef make_instance(JSContextRef ctx, std::shared_ptr<generic_function> func) {
        auto clazz = make_class(func);
        auto instance = JSObjectMake(ctx, clazz, new PrivateDataHolder<generic_function>(func));
        return instance;
    }
};

template <typename T> class NativeFunction : public NativeFunctionBase {};

template <class Return, class... Args> class NativeFunction<Return(Args...)> : public NativeFunctionBase {
  protected:
    using Holder = JSHolder<JSObjectRef>;
    std::shared_ptr<Holder> holder;

  public:
    NativeFunction(JSContextRef ctx, std::shared_ptr<generic_function> func)
        : holder(std::make_shared<Holder>(ctx, make_instance(ctx, func))) {
    }

  public:
    JSObjectRef rawref() const {
        return holder->value;
    }

  protected:
    JSObjectRef thisObj;

  public:
    Return operator()(Args... args) {
        JSObjectRef funcObj = holder->ToObjectRef();
        JSObjectRef thisObj = this->thisObj;
        return js_call<Return, Args...>(holder->ctx, funcObj, thisObj, args...);
    }

  public:
    NativeFunction &operator=(const NativeFunction &other) {
        this->holder = other.holder;
        return *this;
    }
};
} // namespace detail
} // namespace ulbind17

namespace ulbind17 {
namespace detail {

// cast nativefunction to JSValue
template <
    typename FromType, typename ToType,
    typename std::enable_if_t<detail::function_traits<FromType>::value != detail::CppFuntionType::NotFunc> * = nullptr,
    typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    using FuncSignature = function_signature_t<FromType>;
    auto func = to_cpp_function(std::forward<FromType>(value));
    return NativeFunction<FuncSignature>(ctx, func).rawref();
};

} // namespace detail
} // namespace ulbind17
