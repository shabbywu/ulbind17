#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>
#include <string>

#include "jsstring.hpp"
#include "jsvalue.hpp"
#include "nativefunction.hpp"
#include "property.hpp"

namespace ulbind17 {
namespace detail {
class Object : public Value<JSObjectRef> {
  private:
    static JSObjectRef make_empty_object(JSContextRef ctx) {
        return JSObjectMake(ctx, nullptr, nullptr);
    }

  public:
    using Value<JSObjectRef>::Value;
    Object(JSContextRef ctx, JSObjectRef object) : Value(ctx, object) {};
    Object(JSContextRef ctx) : Value(ctx, make_empty_object(ctx)) {};

    static Object GetGlobalObject(JSContextRef ctx) {
        return Object(ctx, JSContextGetGlobalObject(ctx));
    }

  public:
    BoundProperty operator[](std::string propertyName) const {
        return BoundProperty(holder, propertyName);
    }

    BoundProperty operator[](unsigned int propertyIndex) const {
        return BoundProperty(holder, propertyIndex);
    }

  public:
    // getter
    template <typename Return> Return get(std::string propertyName) const {
        return this->operator[](propertyName).value<Return>();
    }

    template <typename Return> Return get(unsigned int propertyIndex) const {
        return this->operator[](propertyIndex).value<Return>();
    }

  public:
    // setter
    template <typename Value>
    void set(std::string propertyName, Value &&v, JSPropertyAttributes attributes = kJSPropertyAttributeNone) {
        this->operator[](propertyName).assign(std::forward<Value>(v), attributes);
    }

    template <typename Value> void set(unsigned int propertyIndex, Value &&v) {
        this->operator[](propertyIndex).assign(std::forward<Value>(v));
    }

    template <typename Value>
    void set(std::string propertyName, Value &v, JSPropertyAttributes attributes = kJSPropertyAttributeNone) {
        this->operator[](propertyName).assign(std::forward<Value>(v), attributes);
    }

    template <typename Value> void set(unsigned int propertyIndex, Value &v) {
        this->operator[](propertyIndex).assign(std::forward<Value>(v));
    }

  public:
    // bindFunc
    template <typename Func, typename FuncSignature = function_signature_t<Func>>
    void bindFunc(std::string property, Func &&v, JSPropertyAttributes attributes = kJSPropertyAttributeNone) {
        auto object = holder->ToObjectRef();
        auto propertyName = generic_cast<std::string, JSStringRef>(holder->ctx, std::forward<std::string>(property));

        auto func = to_cpp_function(std::forward<Func>(v));
        auto value = NativeFunction<FuncSignature>(holder->ctx, func).rawref();
        JSObjectSetProperty(holder->ctx, object, propertyName, value, attributes, nullptr);
    }

    template <typename Func, typename FuncSignature = function_signature_t<Func>>
    void bindFunc(unsigned int propertyIndex, Func &&v) {
        auto object = holder->ToObjectRef();

        auto func = to_cpp_function(std::forward<Func>(v));
        auto value = NativeFunction<FuncSignature>(holder->ctx, func).rawref();
        JSObjectSetPropertyAtIndex(holder->ctx, object, propertyIndex, value, nullptr);
    }
};
} // namespace detail

} // namespace ulbind17
