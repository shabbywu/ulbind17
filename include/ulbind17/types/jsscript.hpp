#pragma once
#include "jsstring.hpp"
#include "ulbind17/cast.hpp"
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>

namespace ulbind17 {
namespace detail {
class Script {
  public:
    Script(JSContextRef ctx, std::string script) : ctx(ctx), script(ctx, script) {
    }

  public:
    template <typename Return>
    std::enable_if_t<!std::is_void_v<Return>, Return> Evaluate(JSObjectRef thisObject = nullptr) {
        JSValueRef exception = 0;
        JSValueRef result = JSEvaluateScript(ctx, script.rawref(), thisObject, nullptr, 0, &exception);
        if (exception) {
            std::string message = "Evaluate Error: ";
            message += String(ctx, JSValueToStringCopy(ctx, exception, nullptr)).value().c_str();
            throw std::runtime_error(message);
        }
        return generic_cast<JSValueRef, Return>(ctx, std::forward<JSValueRef>(result));
    }

    template <typename Return>
    std::enable_if_t<std::is_void_v<Return>> Evaluate(JSObjectRef thisObject = nullptr) {
        JSValueRef exception = 0;
        JSValueRef result = JSEvaluateScript(ctx, script.rawref(), thisObject, nullptr, 0, &exception);
        if (exception) {
            std::string message = "Evaluate Error: ";
            message += String(ctx, JSValueToStringCopy(ctx, exception, nullptr)).value().c_str();
            throw std::runtime_error(message);
        }
    }

  protected:
    JSContextRef ctx;
    String script;
};
} // namespace detail
} // namespace ulbind17
