#pragma once

#include <Ultralight/js/Context.h>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ulbind17 {
namespace js = ultralight::js;

/// Read a property with the SDK's strict conversion and checked error handling.
template <typename T> [[nodiscard]] js::Result<T> get(const js::Value &value, std::string_view key) {
    auto property = value.GetProperty(std::string(key).c_str());
    if (!property)
        return js::Unexpected(std::move(property).error());
    return property.value().template To<T>();
}

template <typename T, std::integral I>
    requires(!std::same_as<I, bool>)
[[nodiscard]] js::Result<T> get(const js::Value &value, I index) {
    return get<T>(value, std::to_string(index));
}

/// Own enumerable string keys, in JavaScript Object.keys order. Does not read values.
[[nodiscard]] inline js::Result<std::vector<std::string>> keys(const js::Value &value) {
    auto alive = value.ToBoolean();
    if (!alive)
        return js::Unexpected(std::move(alive).error());
    if (!value.IsObject())
        return js::Unexpected(js::Error::TypeError("keys() expects an object or array"));
    js::Context context(value);
    auto object = context.GlobalObject().GetProperty("Object");
    if (!object)
        return js::Unexpected(std::move(object).error());
    auto function = object.value().GetProperty("keys");
    if (!function)
        return js::Unexpected(std::move(function).error());
    return function.value().Invoke<std::vector<std::string>>(value);
}

/// Array length (including holes), or the number of an object's own enumerable keys.
[[nodiscard]] inline js::Result<std::size_t> size(const js::Value &value) {
    if (value.IsArray())
        return get<std::size_t>(value, "length");
    auto names = keys(value);
    if (!names)
        return js::Unexpected(std::move(names).error());
    return names.value().size();
}
} // namespace ulbind17
