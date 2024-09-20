#pragma once
#include "jsarray__def.hpp"
#include "jsfunction.hpp"
#include "jsiterator.hpp"

namespace ulbind17 {
namespace detail {
inline std::vector<std::string> Array::keys() const {
    std::vector<std::string> result;
    auto it = Iterator(holder->ctx, get<Function<JSObjectRef()>>("keys")());
    while (true) {
        auto o = it.next();
        if (o.get<bool>("done")) {
            break;
        }
        result.push_back(o.get<std::string>("value"));
    }
    return result;
};
} // namespace detail
} // namespace ulbind17
