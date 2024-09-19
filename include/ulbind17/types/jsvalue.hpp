#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "holder.hpp"

namespace ulbind17 {
namespace detail {
template <typename T = JSValueRef> class Value {
  public:
    std::shared_ptr<Holder<T>> holder;

  public:
    Value(JSContextRef ctx, T ref) : holder(std::make_shared<Holder<T>>(ctx, ref)) {};
    Value(std::shared_ptr<Holder<T>> ref) : holder(ref) {};

  public:
    JSType type() const {
        return holder->type();
    }

    T rawref() const {
        return holder->value;
    }
};
} // namespace detail
} // namespace ulbind17
