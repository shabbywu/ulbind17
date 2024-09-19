#pragma once

#include "ulbind17/types/jsstring.hpp"

namespace ulbind17 {
namespace detail {
// cast JSValue to std::string
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, std::string>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return String(ctx, std::forward<FromType>(value)).value();
};

// cast std::string to JSValue or JSString
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, std::string>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return JSValueMakeString(ctx, String(ctx, std::forward<FromType>(value)).rawref());
};

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, std::string>> * = nullptr,
          typename std::enable_if_t<std::is_same_v<ToType, JSStringRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    return String(ctx, std::forward<FromType>(value)).rawref();
};

} // namespace detail
} // namespace ulbind17
