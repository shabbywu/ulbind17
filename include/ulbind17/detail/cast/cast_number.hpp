#pragma once

#include "ulbind17/types/jsnumber.hpp"
#include <type_traits>

namespace ulbind17 {
namespace detail {
// cast JSValue to arithemetic
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_arithmetic_v<ToType> && !std::is_same_v<ToType, bool>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Number<ToType>(ctx, std::forward<FromType>(value)).value();
};

// cast arithemetic to JSValue
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_arithmetic_v<FromType> && !std::is_same_v<std::decay_t<FromType>, bool>> * =
              nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return Number<FromType>(ctx, value).rawref();
};

// cast arithemetic to JSValue
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_arithmetic_v<FromType> && !std::is_same_v<std::decay_t<FromType>, bool>> * =
              nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, const FromType &&value) {
    return Number<FromType>(ctx, value).rawref();
};

} // namespace detail
} // namespace ulbind17
