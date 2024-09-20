#pragma once

#include <JavaScriptCore/JSRetainPtr.h>

namespace ulbind17 {
namespace detail {

template <typename FromType, typename ToType, typename std::enable_if_t<std::is_void_v<ToType>> * = nullptr>
void generic_cast(JSContextRef ctx, FromType &&value) {};

} // namespace detail
} // namespace ulbind17
