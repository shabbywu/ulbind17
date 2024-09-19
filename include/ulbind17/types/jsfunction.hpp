#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "jsobject.hpp"
#include "ulbind17/detail/function/js_call.hpp"

namespace ulbind17 {
namespace detail {
template <typename T> class Function;

template <class Return, class... Args> class Function<Return(Args...)> : public Object {
  public:
    using Object::Object;

  protected:
    JSObjectRef thisObj;

  public:
    Return operator()(Args... args) {
        JSObjectRef funcObj = holder->ToObjectRef();
        JSObjectRef thisObj = this->thisObj;
        return js_call<Return, Args...>(holder->ctx, funcObj, thisObj, args...);
    }

  public:
    Function &operator=(const Function &other) {
        this->holder = other.holder;
        return *this;
    }
};
} // namespace detail
} // namespace ulbind17
