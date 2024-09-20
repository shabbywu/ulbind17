#pragma once
#include "jsfunction__def.hpp"
#include "jsobject__def.hpp"
#include <memory>

namespace ulbind17 {
namespace detail {
class Iterator : public Object {
  protected:
    using NextFunc = Function<Object()>;

    std::shared_ptr<NextFunc> nextFunc = nullptr;

  public:
    Iterator(JSContextRef ctx, JSObjectRef object) : Object(ctx, object) {
        nextFunc = std::make_shared<NextFunc>(get<NextFunc>("next"));
    };

  public:
    inline Object next();
};
} // namespace detail
} // namespace ulbind17
