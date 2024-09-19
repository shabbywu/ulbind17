#pragma once

#include "ulbind17/types/jsnull.hpp"

namespace ulbind17 {
namespace detail {
// cast JSValue to Null
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSObjectRef> ||
                                    std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, Null>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Null(ctx);
};

// cast Null/nullptr to JSValue
template <
    typename FromType, typename ToType,
    typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, Null>> * = nullptr,
    typename std::enable_if_t<std::is_same_v<ToType, JSObjectRef> || std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return value.rawref();
};

template <
    typename FromType, typename ToType, typename std::enable_if_t<std::is_null_pointer_v<FromType>> * = nullptr,
    typename std::enable_if_t<std::is_same_v<ToType, JSObjectRef> || std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Null(ctx).rawref();
};

} // namespace detail
} // namespace ulbind17
