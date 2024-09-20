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
    std::enable_if_t<!std::is_void_v<Return>, Return> Evaluate(JSObjectRef thisObject = nullptr,
                                                               JSValueRef *exception = nullptr) {
        JSValueRef result = JSEvaluateScript(ctx, script.rawref(), thisObject, nullptr, 0, exception);
        if (exception) {
            throw "failed to evaluate javascript";
        }
        return generic_cast<JSValueRef, Return>(ctx, std::forward<JSValueRef>(result));
    }

    template <typename Return>
    std::enable_if_t<std::is_void_v<Return>> Evaluate(JSObjectRef thisObject = nullptr,
                                                      JSValueRef *exception = nullptr) {
        JSValueRef result = JSEvaluateScript(ctx, script.rawref(), thisObject, nullptr, 0, exception);
        if (exception) {
            throw "failed to evaluate javascript";
        }
    }

  protected:
    JSContextRef ctx;
    String script;
};
} // namespace detail
} // namespace ulbind17
