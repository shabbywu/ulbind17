#pragma once

#include "ulbind17/types/jsundefined.hpp"

namespace ulbind17 {
namespace detail {
// cast JSValue to Undefined
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSObjectRef> ||
                                    std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, Undefined>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Undefined(ctx);
};

// cast Undefined to JSValue
template <
    typename FromType, typename ToType,
    typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, Undefined>> * = nullptr,
    typename std::enable_if_t<std::is_same_v<ToType, JSObjectRef> || std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return value.rawref();
};
} // namespace detail
} // namespace ulbind17
