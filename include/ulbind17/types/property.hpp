#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>
#include <string>

#include "jsvalue.hpp"

namespace ulbind17 {
namespace detail {
class BoundProperty : public Value<JSObjectRef> {
  protected:
    enum class Mode {
        None,
        PropertyName,
        PropertyIndex,
    };

    Mode mode;
    std::string propertyName;
    unsigned int propertyIndex;

  public:
    using Value<JSObjectRef>::Value;
    BoundProperty(decltype(Value::holder) holder, std::string propertyName)
        : Value(holder), propertyName(propertyName), propertyIndex(-1), mode(Mode::PropertyName) {
    }
    BoundProperty(decltype(Value::holder) holder, unsigned int propertyIndex)
        : Value(holder), propertyIndex(propertyIndex), propertyName(""), mode(Mode::PropertyIndex) {
    }

  public:
    // TODO: support const
    template <typename Return = JSValueRef> Return value() const {
        if (mode == Mode::PropertyName) {
            auto object = holder->ToObjectRef();
            auto propertyName = generic_cast<const std::string, JSStringRef>(
                holder->ctx, std::forward<const std::string>(this->propertyName));
            auto p = JSObjectGetProperty(holder->ctx, object, propertyName, nullptr);
            return generic_cast<JSValueRef, Return>(holder->ctx, std::forward<JSValueRef>(p));
        } else {
            auto object = holder->ToObjectRef();
            auto p = JSObjectGetPropertyAtIndex(holder->ctx, object, this->propertyIndex, nullptr);
            return generic_cast<JSValueRef, Return>(holder->ctx, std::forward<JSValueRef>(p));
        }
    }

    template <typename Value = JSValueRef>
    void assign(Value &&v, JSPropertyAttributes attributes = kJSPropertyAttributeNone) {
        if (mode == Mode::PropertyName) {
            auto object = holder->ToObjectRef();
            auto propertyName =
                generic_cast<std::string, JSStringRef>(holder->ctx, std::forward<std::string>(this->propertyName));
            auto value = generic_cast<Value, JSValueRef>(holder->ctx, std::forward<Value>(v));
            JSObjectSetProperty(holder->ctx, object, propertyName, value, attributes, nullptr);
        } else {
            auto object = holder->ToObjectRef();
            auto value = generic_cast<Value, JSValueRef>(holder->ctx, std::forward<Value>(v));
            JSObjectSetPropertyAtIndex(holder->ctx, object, this->propertyIndex, value, nullptr);
        }
    }

  public:
    BoundProperty(const BoundProperty &) = delete;
    BoundProperty &operator=(const BoundProperty &other) {
        if (this->holder != other.holder) {
            assign(other.value());
        } else if ((this->mode == Mode::PropertyName && this->propertyName != other.propertyName) ||
                   (this->mode == Mode::PropertyIndex && this->propertyIndex != other.propertyIndex))
            return *this;
    }

    template <typename T, std::enable_if_t<!std::is_same_v<T, BoundProperty>> * = nullptr>
    BoundProperty &operator=(const T &other) {
        assign(std::forward<const T>(other));
        return *this;
    }
};
} // namespace detail
} // namespace ulbind17
