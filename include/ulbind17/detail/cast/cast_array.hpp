#pragma once

#include "ulbind17/types/jsarray__def.hpp"

namespace ulbind17 {
namespace detail {
// cast JSObjectRef to Array
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSObjectRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, Array>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&from) {
    return Array(ctx, from);
};

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, Array>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&from) {
    JSObjectRef o = JSValueToObject(ctx, from, nullptr);
    return Array(ctx, o);
};

// cast Array to JSValue
template <
    typename FromType, typename ToType,
    typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, Array>> * = nullptr,
    typename std::enable_if_t<std::is_same_v<ToType, JSObjectRef> || std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&from) {
    return from.rawref();
};
} // namespace detail
} // namespace ulbind17
