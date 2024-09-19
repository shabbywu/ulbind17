#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "jsvalue.hpp"

namespace ulbind17 {
namespace detail {
class Null : public Value<> {
  private:
    static JSValueRef make_null(JSContextRef ctx) {
        return JSValueMakeNull(ctx);
    }

  public:
    using Value::Value;
    Null(JSContextRef ctx) : Value(ctx, make_null(ctx)) {};

  public:
    nullptr_t value() const {
        return nullptr;
    }

  public:
    Null &operator=(const Null &other) {
        this->holder = other.holder;
        return *this;
    }

    Null &operator=(std::nullptr_t null) {
        return *this;
    }
};
} // namespace detail
} // namespace ulbind17
