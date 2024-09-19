#pragma once
#include <iostream>
#include <memory>
#include <string>

namespace ulbind17 {
namespace detail {
struct PrivateDataHolderFinalizer {
    virtual ~PrivateDataHolderFinalizer() {};
};

template <typename T, typename... Args> struct PrivateDataHolder : public PrivateDataHolderFinalizer {
    PrivateDataHolder(Args &...args) : data(std::make_shared<T>(args...)) {};
    PrivateDataHolder(std::shared_ptr<T> ptr) : data(ptr) {};
    PrivateDataHolder(T *ptr) : data(std::shared_ptr<T>(ptr)) {};

    virtual ~PrivateDataHolder() {
        std::cout << "~PrivateDataHolder" << typeid(T).name() << std::endl;
        if (free_data != nullptr) {
            free_data(this);
        }
    }

    std::shared_ptr<T> data;
    void (*free_data)(PrivateDataHolder *ptr) = nullptr;

    T &operator*() {
        return *data;
    }
};

static void FinalizePrivateDataHolder(JSObjectRef object) {
    auto p = (PrivateDataHolderFinalizer *)JSObjectGetPrivate(object);
    if (p) {
        delete p;
    }
}
} // namespace detail
} // namespace ulbind17
