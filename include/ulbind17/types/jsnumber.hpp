#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "jsvalue.hpp"

namespace ulbind17 {
namespace detail {
class NumberBase : public Value<> {
  protected:
    static JSValueRef make_number(JSContextRef ctx, double v) {
        return JSValueMakeNumber(ctx, v);
    }

  public:
    using Value::Value;
};

template <typename T> class Number : public NumberBase {
  private:
    static JSValueRef make_number(JSContextRef ctx, double v) {
        return JSValueMakeNumber(ctx, v);
    }

  public:
    using NumberBase::NumberBase;
    Number(JSContextRef ctx, T v) : NumberBase(ctx, make_number(ctx, v)) {};

  public:
    T value() const {
        return (T)holder->ToNumber();
    }

  public:
    Number &operator=(const Number &other) {
        this->holder = other.holder;
        return *this;
    }

    Number &operator=(const T &value) {
        *this = Number(holder->ctx, value);
        return *this;
    }
};
} // namespace detail
} // namespace ulbind17
