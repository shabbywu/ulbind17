#pragma once
#include "jsfunction.hpp"
#include "jsobject__def.hpp"
#include "jsscript.hpp"
#include "nativefunction.hpp"
#include "property.hpp"
#include "ulbind17/cast.hpp"

namespace ulbind17 {
namespace detail {
#pragma region js api
inline unsigned int Object::size() const {
    auto o = GetGlobalObject(holder->ctx).get<Object>("Object");
    return o.get<Function<Array(Object)>>("keys")(holder).size();
};

inline unsigned int Object::length() const {
    auto o = GetGlobalObject(holder->ctx).get<Object>("Object");
    return o.get<Function<Array(Object)>>("keys")(holder).size();
};

inline std::vector<std::string> Object::keys() const {
    std::vector<std::string> result;
    auto o = GetGlobalObject(holder->ctx).get<Object>("Object");
    auto keys = o.get<Function<Array(Object)>>("keys")(holder);
    for (int i = 0; i < keys.size(); i++) {
        result.push_back(keys.get<std::string>(i));
    }
    return result;
}

inline std::string Object::toString() const {
    return get<Function<std::string()>>("toString")();
}
#pragma endregion

BoundProperty Object::operator[](std::string propertyName) const {
    return BoundProperty(holder, propertyName);
}

BoundProperty Object::operator[](unsigned int propertyIndex) const {
    return BoundProperty(holder, propertyIndex);
}

#pragma region self-reflection api
inline bool Object::isFunction() const {
    return Script(holder->ctx, "typeof this == 'function'").Evaluate<bool>(rawref());
}

#pragma endregion

#pragma region object::contains
inline bool Object::contains(std::string propertyName) const {
    return !JSValueIsUndefined(holder->ctx, this->operator[](propertyName).value<JSValueRef>());
}

inline bool Object::contains(unsigned int propertyIndex) const {
    return !JSValueIsUndefined(holder->ctx, this->operator[](propertyIndex).value<JSValueRef>());
}
#pragma endregion

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
