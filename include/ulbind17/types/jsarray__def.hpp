#pragma once
#include "jsobject__def.hpp"
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>
#include <string>
#include <vector>

namespace ulbind17 {
namespace detail {
class Array : public Object {
  private:
    static JSObjectRef make_empty_object(JSContextRef ctx) {
        return JSObjectMakeArray(ctx, 0, nullptr, nullptr);
    }

  public:
    using Object::Object;
    Array(JSContextRef ctx) : Object(ctx, make_empty_object(ctx)) {};

  public:
    std::nullptr_t value() const {
        return nullptr;
    }

  public:
    Array &operator=(const Array &other) {
        this->holder = other.holder;
        return *this;
    }

#pragma region js api
  public:
    virtual unsigned int size() const {
        return Object::get<unsigned int>("length");
    }

    virtual unsigned int length() const {
        return Object::get<unsigned int>("length");
    }

    virtual inline std::vector<std::string> keys() const;

#pragma endregion
};

template <typename T> class TypedArray : public Array {
  public:
    TypedArray &operator=(const TypedArray &other) {
        this->holder = other.holder;
        return *this;
    }

  public:
    T get(unsigned int propertyIndex) const {
        return Object::get<T>(propertyIndex);
    }

  public:
    void set(unsigned int propertyIndex, T &v) {
        Object::set<T>(propertyIndex, v);
    }

    void set(unsigned int propertyIndex, T &&v) {
        Object::set<T>(propertyIndex, v);
    }
};
} // namespace detail
} // namespace ulbind17
