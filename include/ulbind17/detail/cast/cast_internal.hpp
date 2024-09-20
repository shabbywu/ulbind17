#pragma once

#include "ulbind17/types/jsobject__def.hpp"
#include <JavaScriptCore/JSObjectRef.h>

namespace ulbind17 {
namespace detail {
// cast JSObjectRef/JSValueRef To JSValueRef
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSObjectRef> ||
                                    std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&from) {
    return from;
};

// cast JSValueRef To JSObjectRef
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSObjectRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&from) {
    return JSValueToObject(ctx, from, nullptr);
};

// cast Object to JSValueRef/JSObjectRef

template <
    typename FromType, typename ToType,
    typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, Object>> * = nullptr,
    typename std::enable_if_t<std::is_same_v<ToType, JSObjectRef> || std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&from) {
    return from.rawref();
};

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, Object>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&from) {
    auto object = JSValueToObject(ctx, from, nullptr);
    return Object(ctx, object);
};

} // namespace detail
} // namespace ulbind17
