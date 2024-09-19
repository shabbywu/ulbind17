#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>
#include <string>

namespace ulbind17 {
namespace detail {
static inline std::string exception_to_string(JSContextRef ctx, JSValueRef exception) {
    auto message = JSValueToStringCopy(ctx, exception, nullptr);
    auto jsstring = adopt(message);
    std::string buffer;
    auto size = JSStringGetMaximumUTF8CStringSize(jsstring.get());
    buffer.resize(size);
    JSStringGetUTF8CString(jsstring.get(), (char *)buffer.data(), size);
    return buffer;
}

template <typename T> class Holder;

template <typename T = JSValueRef> class Holder {
  public:
    Holder(JSContextRef ctx, T value) : ctx(ctx), value(value) {
        JSValueProtect(ctx, value);
    }
    ~Holder() {
        JSValueUnprotect(ctx, value);
    }
    JSContextRef ctx;
    T value;

  public:
    JSType type() {
        return JSValueGetType(ctx, value);
    }

    JSObjectRef ToObjectRef() {
        return value;
    }

    JSRetainPtr<JSStringRef> ToStringRef() {
        JSValueRef exception = nullptr;
        auto ref = adopt(JSValueToStringCopy(ctx, value, &exception));
        if (exception) {
            throw std::exception("cast exception");
        }
        return ref;
    }

    double ToNumber() {
        JSValueRef exception = nullptr;
        auto number = JSValueToNumber(ctx, value, &exception);
        if (exception) {
            throw std::exception("cast exception");
        }
        return number;
    }

    bool ToBoolean() {
        return JSValueToBoolean(ctx, value);
    }
};

template <> class Holder<JSClassRef> {
  public:
    Holder(JSContextRef ctx, JSClassRef value) : ctx(ctx), value(value) {
        if (value) {
            JSClassRetain(value);
        }
    }
    ~Holder() {
        if (value) {
            JSClassRelease(value);
        }
    }
    JSContextRef ctx;
    JSClassRef value;
};

} // namespace detail
} // namespace ulbind17
