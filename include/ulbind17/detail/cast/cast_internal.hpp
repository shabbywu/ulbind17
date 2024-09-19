#pragma once

#include "ulbind17/types/jsstring.hpp"

namespace ulbind17 {
namespace detail {
// cast JSObjectRef/JSValueRef To JSValueRef
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSObjectRef> ||
                                    std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return value;
};

// cast JSValueRef To JSObjectRef
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSObjectRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return JSValueToObject(ctx, value, nullptr);
};

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, Object>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return value.rawref();
};

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, Object>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Object(ctx, value);
};

} // namespace detail
} // namespace ulbind17
