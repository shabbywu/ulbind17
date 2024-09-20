#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

#include "boundable.hpp"
#include "jsobject__def.hpp"

namespace ulbind17 {
namespace detail {
class JSFunctionBase : public Boundable {
  public:
    using Boundable::Boundable;
};

template <typename T> class Function;

template <class Return, class... Args> class Function<Return(Args...)> : public Object, public JSFunctionBase {
  public:
    Function(JSContextRef ctx, JSObjectRef object) : Object(ctx, object), JSFunctionBase(ctx, nullptr) {};

  public:
    JSObjectRef thisObj;

  public:
    inline Return operator()(Args... args);

  public:
    Function &operator=(const Function &other) {
        Object::holder = other.Object::holder;
        Boundable::holder = other.Boundable::holder;
        return *this;
    }
};

#pragma region cast JSValueRef/JSObjectRef to Function
template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSValueRef>> * = nullptr,
          typename std::enable_if_t<std::is_base_of_v<JSFunctionBase, std::decay_t<ToType>>> * = nullptr>
static ToType generic_cast(JSContextRef ctx, FromType &&from) {
    auto object = JSValueToObject(ctx, from, nullptr);
    return ToType(ctx, object);
}

template <typename FromType, typename ToType,
          typename std::enable_if_t<std::is_same_v<std::decay_t<FromType>, JSObjectRef>> * = nullptr,
          typename std::enable_if_t<std::is_base_of_v<JSFunctionBase, std::decay_t<ToType>>> * = nullptr>
static ToType generic_cast(JSContextRef ctx, FromType &&from) {
    return ToType(ctx, from);
}
#pragma endregion

} // namespace detail
} // namespace ulbind17
