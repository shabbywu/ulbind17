#pragma once
#include "jsfunction__def.hpp"
#include "ulbind17/cast.hpp"
#include "ulbind17/detail/function/js_call.hpp"

namespace ulbind17 {
namespace detail {
template <class Return, class... Args> inline Return Function<Return(Args...)>::operator()(Args... args) {
    JSObjectRef funcObj = Object::holder->ToObjectRef();
    return js_call<Return, Args...>(Object::holder->ctx, funcObj, Boundable::holder->ToObjectRef(), args...);
}
} // namespace detail
} // namespace ulbind17
