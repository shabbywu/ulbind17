#pragma once
#include "jsclass__def.hpp"
#include "nativefunction.hpp"

namespace ulbind17 {
namespace detail {
template <typename C> template <typename Func> void Class<C>::bindFunc(std::string propertyName, Func &&func) {
    auto container = to_cpp_function(std::forward<Func>(func));
    JSObjectRef callback = NativeFunction<detail::function_signature_t<Func>>(holder->ctx, container).rawref();
    std::function<PropertyGetter> fget = [=](C *self, JSContextRef ctx, JSValueRef *exception) { return callback; };
    getter[propertyName] = fget;
}

} // namespace detail
} // namespace ulbind17
