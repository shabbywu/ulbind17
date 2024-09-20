#pragma once
#include "jsstring.hpp"
#include "jsvalue.hpp"
#include "ulbind17/detail/function/type_traits.hpp"
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>
#include <string>

namespace ulbind17 {
namespace detail {

class BoundProperty;

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
    inline BoundProperty operator[](std::string propertyName) const;

    inline BoundProperty operator[](unsigned int propertyIndex) const;

  public:
    // getter
    template <typename Return> inline Return get(std::string propertyName) const;

    template <typename Return> inline Return get(unsigned int propertyIndex) const;

  public:
    // setter
    template <typename Value>
    inline void set(std::string propertyName, Value &&v, JSPropertyAttributes attributes = kJSPropertyAttributeNone);

    template <typename Value> inline void set(unsigned int propertyIndex, Value &&v);

    template <typename Value>
    inline void set(std::string propertyName, Value &v, JSPropertyAttributes attributes = kJSPropertyAttributeNone);

    template <typename Value> inline void set(unsigned int propertyIndex, Value &v);

  public:
    // bindFunc
    template <typename Func, typename FuncSignature = function_signature_t<Func>>
    void bindFunc(std::string property, Func &&v, JSPropertyAttributes attributes = kJSPropertyAttributeNone);

    template <typename Func, typename FuncSignature = function_signature_t<Func>>
    void bindFunc(unsigned int propertyIndex, Func &&v);
};
} // namespace detail

} // namespace ulbind17
