#pragma once
#include "jsvalue.hpp"
namespace ulbind17 {
namespace detail {
class Boundable : public Value<JSObjectRef> {
  public:
    using Value<JSObjectRef>::Value;

  public:
    void bindTo(decltype(holder) holder) {
        this->holder = holder;
    }
};
} // namespace detail
} // namespace ulbind17
