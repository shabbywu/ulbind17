#pragma once
#include <functional>
#include <tuple>

namespace ulbind17 {
namespace detail {
// load_args will load args from <js call c++ context> into cpp memory
template <int index, class ReturnType> struct load_args;

// default implement, will load args as std::tuple
//
// Example to call cpp_func with args in javascripts
// template<class Return, class... Args>
// JSValueRef caller(Return(*func)(Args...), JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t
// argumentCount, const JSValueRef arguments[], JSValueRef *exception) {
//    std::tuple<Args...> args = load_args<Args...>(ctx, arguments);
//    return generic_cast<Return, JSValueRef>(ctx, std::apply(func, args));
// }
template <int index, typename Arg, typename... Args> struct load_args<index, std::tuple<Arg, Args...>> {
    static std::tuple<Arg, Args...> load(JSContextRef ctx, const JSValueRef arguments[]) {
        return std::tuple_cat(load_args<index, std::tuple<Arg>>::load(ctx, arguments),
                              load_args<index + 1, std::tuple<Args...>>::load(ctx, arguments));
    }
};

template <int index, typename Arg> struct load_args<index, std::tuple<Arg>> {
    static std::tuple<Arg> load(JSContextRef ctx, const JSValueRef arguments[]) {
        Arg arg = generic_cast<const JSValueRef, Arg>(ctx, std::forward<const JSValueRef>(arguments[index]));
        return std::make_tuple<Arg>(std::forward<Arg>(arg));
    }
};

template <int index> struct load_args<index, std::tuple<>> {
    static std::tuple<> load(JSContextRef ctx, const JSValueRef arguments[]) {
        return std::make_tuple();
    }
};
} // namespace detail
} // namespace ulbind17
