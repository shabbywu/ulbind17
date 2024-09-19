#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "jsvalue.hpp"

namespace ulbind17 {
namespace detail {
class Boolean : public Value<> {
  private:
    static JSValueRef make_boolean(JSContextRef ctx, bool boolean) {
        return JSValueMakeBoolean(ctx, boolean);
    }

  public:
    using Value::Value;
    Boolean(JSContextRef ctx, bool boolean) : Value(ctx, make_boolean(ctx, boolean)) {};

  public:
    bool value() const {
        return holder->ToBoolean();
    }

  public:
    Boolean &operator=(const Boolean &other) {
        this->holder = other.holder;
        return *this;
    }

    Boolean &operator=(const bool &boolean) {
        *this = Boolean(holder->ctx, boolean);
        return *this;
    }
};
} // namespace detail
} // namespace ulbind17
