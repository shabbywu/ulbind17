#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <functional>

namespace ulbind17 {
namespace detail {
inline void js_call_setup_args(JSContextRef ctx, JSValueRef *arguments) {
}

template <class Arg, class... Args>
inline void js_call_setup_args(JSContextRef ctx, JSValueRef *arguments, Arg &&head, Args... tail) {
    arguments[0] = generic_cast<Arg, JSValueRef>(ctx, std::forward<Arg>(head));
    js_call_setup_args(ctx, arguments + 1, tail...);
}

template <class... Args> inline void js_call_setup_args(JSContextRef ctx, JSValueRef *arguments, Args... args) {
    js_call_setup_args(ctx, arguments, args...);
}

template <class Return, class... Args>
inline std::enable_if_t<!std::is_void_v<Return>, Return> js_call(JSContextRef ctx, JSObjectRef funcObj,
                                                                 JSObjectRef thisObj, Args... args) {
    JSValueRef *arguments = nullptr;
    if (sizeof...(args) >= 1) {
        arguments = new JSValueRef[sizeof...(args)];
        js_call_setup_args(ctx, arguments, args...);
    }

    JSValueRef exception = 0;
    JSValueRef result = JSObjectCallAsFunction(ctx, funcObj, thisObj, sizeof...(args), arguments, &exception);
    if (arguments != nullptr)
        delete[] arguments;
    if (exception) {
        throw "...";
    } else {
        return generic_cast<JSValueRef, Return>(ctx, std::forward<JSValueRef>(result));
    }
}

template <class Return, class... Args>
inline std::enable_if_t<std::is_void_v<Return>> js_call(JSContextRef ctx, JSObjectRef funcObj, JSObjectRef thisObj,
                                                        Args... args) {
    JSValueRef *arguments = nullptr;
    if (sizeof...(args) >= 1) {
        arguments = new JSValueRef[sizeof...(args)];
        js_call_setup_args(ctx, arguments, args...);
    }

    JSValueRef exception = 0;
    JSValueRef result = JSObjectCallAsFunction(ctx, funcObj, thisObj, sizeof...(args), arguments, &exception);
    if (arguments != nullptr)
        delete[] arguments;
    if (exception) {
        throw "...";
    } else {
    }
}

} // namespace detail
} // namespace ulbind17
