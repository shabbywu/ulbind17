#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "jsvalue.hpp"

namespace ulbind17 {
namespace detail {
class Undefined : public Value<> {
  private:
    static JSValueRef make_undefined(JSContextRef ctx) {
        return JSValueMakeUndefined(ctx);
    }

  public:
    using Value::Value;
    Undefined(JSContextRef ctx) : Value(ctx, make_undefined(ctx)) {};

  public:
    std::nullptr_t value() const {
        return nullptr;
    }

  public:
    Undefined &operator=(const Undefined &other) {
        this->holder = other.holder;
        return *this;
    }

    Undefined &operator=(std::nullptr_t null) {
        return *this;
    }
};
} // namespace detail
} // namespace ulbind17
