#pragma once

#include "ulbind17/types/jsclass__def.hpp"
#include "ulbind17/types/jsstring.hpp"

namespace ulbind17 {
namespace detail {
// cast pointer to JSObjectRef/JSValueRef
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_class_v<std::remove_pointer_t<FromType>> && std::is_pointer_v<FromType> &&
                                    !(std::is_same_v<std::decay_t<FromType>, JSValueRef> ||
                                      std::is_same_v<std::decay_t<FromType>, JSObjectRef>)> * = nullptr,
          typename std::enable_if_t<std::is_same_v<std::decay_t<ToType>, JSObjectRef> ||
                                    std::is_same_v<std::decay_t<ToType>, JSValueRef>> * = nullptr>
static ToType generic_cast(JSContextRef ctx, FromType &&from) {
    using TClass = std::remove_pointer_t<FromType>;
    auto clazz = detail::ClassRegistry::getIntance().findJSClass<TClass>();
    return clazz->makeInstance(ctx, from);
}

// cast JSObjectRef/JSValueRef to pointer
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSObjectRef>> * = nullptr,
          typename std::enable_if_t<std::is_class_v<std::remove_pointer_t<ToType>> && std::is_pointer_v<ToType> &&
                                    !(std::is_same_v<std::decay_t<ToType>, JSValueRef> ||
                                      std::is_same_v<std::decay_t<ToType>, JSObjectRef>)> * = nullptr>
static ToType generic_cast(JSContextRef ctx, FromType &&from) {
    return JSObjectGetPrivate(from);
}

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_class_v<std::remove_pointer_t<ToType>> && std::is_pointer_v<ToType> &&
                                    !(std::is_same_v<std::decay_t<ToType>, JSValueRef> ||
                                      std::is_same_v<std::decay_t<ToType>, JSObjectRef>)> * = nullptr>
static ToType generic_cast(JSContextRef ctx, FromType &&from) {
    auto object = JSValueToObject(ctx, from, nullptr);
    return (ToType)JSObjectGetPrivate(object);
}

} // namespace detail
} // namespace ulbind17
