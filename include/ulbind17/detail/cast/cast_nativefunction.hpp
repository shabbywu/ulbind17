#pragma once

#include "ulbind17/detail/function/cpp_function.hpp"
#include "ulbind17/types/nativefunction.hpp"

namespace ulbind17 {
namespace detail {

// cast nativefunction to JSValue
template <
    typename FromType, typename ToType,
    typename std::enable_if_t<detail::function_traits<FromType>::value != detail::CppFuntionType::NotFunc> * = nullptr,
    typename std::enable_if_t<std::is_same_v<ToType, JSValueRef>> * = nullptr>
ToType generic_cast(JSContextRef ctx, FromType &&value) {
    using FuncSignature = function_signature_t<FromType>;
    auto func = to_cpp_function(std::forward<FromType>(value));
    return NativeFunction<FuncSignature>(ctx, func).rawref();
};

} // namespace detail
} // namespace ulbind17
