#pragma once
#include "jsfunction.hpp"
#include "jsiterator__def.hpp"
namespace ulbind17 {
namespace detail {
inline Object Iterator::next() {
    return nextFunc->operator()();
}
} // namespace detail
} // namespace ulbind17
