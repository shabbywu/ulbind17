#pragma once
#include "jsobject__def.hpp"
#include "nativefunction.hpp"
#include "property.hpp"

namespace ulbind17 {
namespace detail {
BoundProperty Object::operator[](std::string propertyName) const {
    return BoundProperty(holder, propertyName);
}

BoundProperty Object::operator[](unsigned int propertyIndex) const {
    return BoundProperty(holder, propertyIndex);
}

#pragma region object::getter
template <typename Return> Return Object::get(std::string propertyName) const {
    return this->operator[](propertyName).value<Return>();
}

template <typename Return> Return Object::get(unsigned int propertyIndex) const {
    return this->operator[](propertyIndex).value<Return>();
}
#pragma endregion

#pragma region object::setter
template <typename V> void Object::set(std::string propertyName, V &&v, JSPropertyAttributes attributes) {
    this->operator[](propertyName).assign(std::forward<V>(v), attributes);
}

template <typename V> void Object::set(unsigned int propertyIndex, V &&v) {
    this->operator[](propertyIndex).assign(std::forward<V>(v));
}

template <typename V> void Object::set(std::string propertyName, V &v, JSPropertyAttributes attributes) {
    this->operator[](propertyName).assign(std::forward<V>(v), attributes);
}

template <typename V> void Object::set(unsigned int propertyIndex, V &v) {
    this->operator[](propertyIndex).assign(std::forward<V>(v));
}
#pragma endregion

#pragma region bindFunc
template <typename Func, typename FuncSignature>
void Object::bindFunc(std::string property, Func &&v, JSPropertyAttributes attributes) {
    auto object = holder->ToObjectRef();
    auto propertyName = generic_cast<std::string, JSStringRef>(holder->ctx, std::forward<std::string>(property));

    auto func = to_cpp_function(std::forward<Func>(v));
    auto value = NativeFunction<FuncSignature>(holder->ctx, func).rawref();
    JSObjectSetProperty(holder->ctx, object, propertyName, value, attributes, nullptr);
}

template <typename Func, typename FuncSignature> void Object::bindFunc(unsigned int propertyIndex, Func &&v) {
    auto object = holder->ToObjectRef();

    auto func = to_cpp_function(std::forward<Func>(v));
    auto value = NativeFunction<FuncSignature>(holder->ctx, func).rawref();
    JSObjectSetPropertyAtIndex(holder->ctx, object, propertyIndex, value, nullptr);
}
#pragma endregion

} // namespace detail
} // namespace ulbind17
