#pragma once

#include "ulbind17/types/jsboolean.hpp"
#include <type_traits>

namespace ulbind17 {
namespace detail {
// cast JSValue to bool
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, bool>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Boolean(ctx, value).value();
};

// cast bool/Boolean to JSValue
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, bool>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Boolean(ctx, value).rawref();
};

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, Boolean>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return value.rawref();
};

} // namespace detail
} // namespace ulbind17
