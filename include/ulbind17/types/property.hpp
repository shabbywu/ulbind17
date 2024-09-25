#pragma once
#include "boundable.hpp"
#include "jsvalue.hpp"
#include "ulbind17/cast.hpp"
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>
#include <string>

namespace ulbind17 {
namespace detail {
class BoundProperty : public Boundable {
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
    BoundProperty(decltype(BoundProperty::holder) holder, std::string propertyName)
        : Boundable(holder), propertyName(propertyName), propertyIndex(-1), mode(Mode::PropertyName) {
    }
    BoundProperty(decltype(BoundProperty::holder) holder, unsigned int propertyIndex)
        : Boundable(holder), propertyIndex(propertyIndex), propertyName(""), mode(Mode::PropertyIndex) {
    }

  public:
    // TODO: support const
    template <typename Return = JSValueRef> Return value() const {
        if (mode == Mode::PropertyName) {
            auto object = holder->ToObjectRef();
            auto propertyName = generic_cast<const std::string, JSStringRef>(
                holder->ctx, std::forward<const std::string>(this->propertyName));
            auto p = JSObjectGetProperty(holder->ctx, object, propertyName, nullptr);
            Return v = generic_cast<JSValueRef, Return>(holder->ctx, std::forward<JSValueRef>(p));
            if constexpr (std::is_base_of_v<Boundable, Return>) {
                v.bindTo(holder);
            }
            return v;
        } else {
            auto object = holder->ToObjectRef();
            auto p = JSObjectGetPropertyAtIndex(holder->ctx, object, this->propertyIndex, nullptr);
            Return v = generic_cast<JSValueRef, Return>(holder->ctx, std::forward<JSValueRef>(p));
            if constexpr (std::is_base_of_v<Boundable, Return>) {
                v.bindTo(holder);
            }
            return v;
        }
    }

    template <typename V = JSValueRef> void assign(V &&v, JSPropertyAttributes attributes = kJSPropertyAttributeNone) {
        if (mode == Mode::PropertyName) {
            auto object = holder->ToObjectRef();
            auto propertyName =
                generic_cast<std::string, JSStringRef>(holder->ctx, std::forward<std::string>(this->propertyName));
            auto value = generic_cast<V, JSValueRef>(holder->ctx, std::forward<V>(v));
            JSObjectSetProperty(holder->ctx, object, propertyName, value, attributes, nullptr);
        } else {
            auto object = holder->ToObjectRef();
            auto value = generic_cast<V, JSValueRef>(holder->ctx, std::forward<V>(v));
            JSObjectSetPropertyAtIndex(holder->ctx, object, this->propertyIndex, value, nullptr);
        }
    }

  public:
    BoundProperty(const BoundProperty &) = delete;
    BoundProperty &operator=(const BoundProperty &other) {
        if (this->holder != other.holder) {
            assign(other.value());
        } else if ((this->mode == Mode::PropertyName && this->propertyName != other.propertyName) ||
                   (this->mode == Mode::PropertyIndex && this->propertyIndex != other.propertyIndex)) {
            // TODO
        }
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
