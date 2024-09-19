#pragma once
#include <JavaScriptCore/JSObjectRef.h>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "ulbind17/detail/function/js_call.hpp"
#include "ulbind17/detail/function/load_args.hpp"
#include "ulbind17/detail/function/type_traits.hpp"
#include "ulbind17/detail/malloc.hpp"

namespace ulbind17 {
namespace detail {
class generic_function {
  protected:
    using Caller = JSValueRef(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);

  public:
    virtual std::function<Caller> get_caller_impl() = 0;
    virtual Caller *get_static_caller() = 0;
    virtual int get_nargs() = 0;
};

class cpp_function : public generic_function {
  protected:
    struct Holder {
        /// Storage for the wrapped function pointer and captured data, if any
        void *data[3] = {};

        /// Pointer to custom destructor for 'data' (if needed)
        void (*free_data)(Holder *ptr) = nullptr;

        /// Pointer to js caller
        std::function<Caller> caller = nullptr;

        bool functor = false;

        ~Holder() {
#ifdef TRACE_CONTAINER_GC
            std::cout << "GC::Release " << typeid(Holder).name() << std::endl;
#endif
            if (free_data != nullptr) {
                free_data(this);
            }
        }
    };

  protected:
    std::shared_ptr<Holder> holder;
    int nargs;
    bool withThisObject = false;

  public:
    virtual std::function<Caller> get_caller_impl() {
        return holder->caller;
    }
    virtual int get_nargs() {
        return nargs;
    }
    virtual Caller *get_static_caller() {
        return &cpp_function::caller;
    }

  public:
    cpp_function(std::nullptr_t) {
    }

    /// Construct a cpp_function from a vanilla function pointer
    template <typename Return, typename... Args>
    // NOLINTNEXTLINE(google-explicit-constructor)
    cpp_function(Return (*func)(Args...)) {
        initialize<false>(func, func);
        nargs = sizeof...(Args);
        withThisObject = false;
    }

    /// Construct a cpp_function from a class method (non-const, no ref-qualifier)
    template <typename Return, typename Class, typename... Args>
    // NOLINTNEXTLINE(google-explicit-constructor)
    cpp_function(Return (Class::*f)(Args...)) {
        initialize<true>(
            std::function([f](Class *c, Args... args) -> Return { return (c->*f)(std::forward<Args>(args)...); }),
            (Return(*)(Class *, Args...)) nullptr);
        nargs = sizeof...(Args);
        withThisObject = true;
    }

    /// Construct a cpp_function from a class method (non-const, lvalue ref-qualifier)
    /// A copy of the overload for non-const functions without explicit ref-qualifier
    /// but with an added `&`.
    template <typename Return, typename Class, typename... Args>
    // NOLINTNEXTLINE(google-explicit-constructor)
    cpp_function(Return (Class::*f)(Args...) &) {
        initialize<true>(
            std::function([f](Class *c, Args... args) -> Return { return (c->*f)(std::forward<Args>(args)...); }),
            (Return(*)(Class *, Args...)) nullptr);
        nargs = sizeof...(Args);
        withThisObject = true;
    }

    /// Construct a cpp_function from a class method (const, no ref-qualifier)
    template <typename Return, typename Class, typename... Args>
    // NOLINTNEXTLINE(google-explicit-constructor)
    cpp_function(Return (Class::*f)(Args...) const) {
        initialize<true>(
            std::function([f](const Class *c, Args... args) -> Return { return (c->*f)(std::forward<Args>(args)...); }),
            (Return(*)(const Class *, Args...)) nullptr);
        nargs = sizeof...(Args);
        withThisObject = true;
    }

    /// Construct a cpp_function from a class method (const, lvalue ref-qualifier)
    /// A copy of the overload for const functions without explicit ref-qualifier
    /// but with an added `&`.
    template <typename Return, typename Class, typename... Args>
    // NOLINTNEXTLINE(google-explicit-constructor)
    cpp_function(Return (Class::*f)(Args...) const &) {
        initialize<true>(
            std::function([f](const Class *c, Args... args) -> Return { return (c->*f)(std::forward<Args>(args)...); }),
            (Return(*)(const Class *, Args...)) nullptr);
        nargs = sizeof...(Args);
        withThisObject = true;
    }

    /// Construct a cpp_function from a lambda function (possibly with internal state)
    template <class Func, typename = std::enable_if_t<function_traits<std::remove_reference_t<Func>>::value ==
                                                      CppFuntionType::LambdaLike>>
    cpp_function(Func &&func) {
        // initialize(std::forward<Func>(func), (typename function_traits<Func>::type*)nullptr);
        initialize<true>(std::function<function_signature_t<Func>>(std::forward<Func>(func)),
                         (function_signature_t<Func> *)nullptr);
        nargs = function_traits<std::remove_reference_t<Func>>::nargs;
        withThisObject = false;
    }

  private:
    /// Special internal constructor for functors, lambda functions, etc.
    template <bool functor, class Func, typename Return, typename... Args>
    void initialize(Func &&f, Return (*signature)(Args...)) {
        struct capture {
            std::remove_reference_t<Func> f;
        };
        holder = std::make_shared<Holder>();
        holder->functor = functor;

        /* Store the capture object directly in the function record if there is enough space */
        if (sizeof(capture) <= sizeof(holder->data)) {
            new ((capture *)&holder->data) capture{std::forward<Func>(f)};
        } else {
            holder->data[0] = new capture{std::forward<Func>(f)};
            holder->free_data = [](Holder *r) { delete ((capture *)r->data[0]); };
        }

        holder->caller = build_caller<Func, Return, Args...>(std::forward<Func>(f));
    }

  private:
    template <class Func, typename Return, typename... Args>
    std::function<Caller> build_caller(Func &&f, Return (*signature)(Args...) = nullptr,
                                       typename std::enable_if_t<std::is_void_v<Return>, Return> * = nullptr) {
        return std::function<Caller>([f, this](JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                               size_t argumentCount, const JSValueRef arguments[],
                                               JSValueRef *exception) -> JSValueRef {
            std::vector<JSValueRef> realArguments;
            if (withThisObject) {
                realArguments.push_back(
                    generic_cast<JSObjectRef, JSValueRef>(ctx, std::forward<JSObjectRef>(thisObject)));
                for (int i = 0; i < argumentCount; i++) {
                    realArguments.push_back(arguments[i]);
                }
            }
            if (argumentCount < nargs) {
                if (exception) {
                    auto message = adopt(JSStringCreateWithUTF8CString("argusments not enough"));
                    *exception = JSValueMakeString(ctx, message.get());
                }
                return JSValueMakeNull(ctx);
            }

            auto args = load_args<0, std::tuple<Args...>>::load(ctx, withThisObject ? realArguments.data() : arguments);
            try {
                if (holder->functor) {
                    auto func = (std::function<Return(Args...)> *)holder->data[0];
                    std::apply(*func, args);
                } else {
                    auto func = (Return(*)(Args...))holder->data[0];
                    std::apply(*func, args);
                }
            }
            catch (const std::exception &e) {
                if (exception) {
                    auto message = adopt(JSStringCreateWithUTF8CString(e.what()));
                    *exception = JSValueMakeString(ctx, message.get());
                }
            }
            return JSValueMakeNull(ctx);
        });
    }

    template <class Func, typename Return, typename... Args>
    std::function<Caller> build_caller(Func &&f, Return (*signature)(Args...) = nullptr,
                                       typename std::enable_if_t<!std::is_void_v<Return>, Return> * = nullptr) {
        return std::function<Caller>([f, this](JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                               size_t argumentCount, const JSValueRef arguments[],
                                               JSValueRef *exception) -> JSValueRef {
            std::vector<JSValueRef> realArguments;
            if (withThisObject) {
                realArguments.push_back(
                    generic_cast<JSObjectRef, JSValueRef>(ctx, std::forward<JSObjectRef>(thisObject)));
                for (int i = 0; i < argumentCount; i++) {
                    realArguments.push_back(arguments[i]);
                }
            }
            if (argumentCount < nargs) {
                if (exception) {
                    auto message = adopt(JSStringCreateWithUTF8CString("argusments not enough"));
                    *exception = JSValueMakeString(ctx, message.get());
                }
                return JSValueMakeNull(ctx);
            }

            auto args = load_args<0, std::tuple<Args...>>::load(ctx, withThisObject ? realArguments.data() : arguments);
            try {
                if (holder->functor) {
                    auto func = (std::function<Return(Args...)> *)holder->data[0];
                    Return v = std::apply(*func, args);
                    return generic_cast<Return, JSValueRef>(ctx, std::forward<Return>(v));
                } else {
                    auto func = (Return(*)(Args...))holder->data[0];
                    Return v = std::apply(*func, args);
                    return generic_cast<Return, JSValueRef>(ctx, std::forward<Return>(v));
                }
            }
            catch (const std::exception &e) {
                if (exception) {
                    auto message = adopt(JSStringCreateWithUTF8CString(e.what()));
                    *exception = JSValueMakeString(ctx, message.get());
                }
            }
            return JSValueMakeNull(ctx);
        });
    }

  public:
    template <typename Return, typename... Args> Return operator()(Args... args) {
        if (holder->functor) {
            auto func = (std::function<Return(Args...)> *)holder->data[0];
            return (*func)(args...);
        } else {
            auto func = (Return(*)(Args...))holder->data[0];
            return (*func)(args...);
        }
    }

  public:
    static JSValueRef caller(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount,
                             const JSValueRef arguments[], JSValueRef *exception) {
        PrivateDataHolder<cpp_function> *h = (PrivateDataHolder<cpp_function> *)JSObjectGetPrivate(function);
        return (**h).holder->caller(ctx, function, thisObject, argumentCount, arguments, exception);
    }
};

template <typename Func> static std::shared_ptr<generic_function> to_cpp_function(Func &&func) {
    return std::make_shared<cpp_function>(std::forward<Func>(func));
}

} // namespace detail
namespace detail {
class overloaded_function : public generic_function {
  private:
    std::map<size_t, std::shared_ptr<generic_function>> callers;

  public:
    template <typename Func> void add_caller(Func &&func) {
        // TODO: rewrite with emplace
        auto caller = to_cpp_function(std::forward<Func>(func));
        callers[caller->get_nargs()] = caller;
    }

    void add_caller(std::shared_ptr<generic_function> caller) {
        callers[caller->get_nargs()] = caller;
    }

  public:
    virtual std::function<Caller> get_caller_impl() {
        return std::function<Caller>([this](JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                                            size_t argumentCount, const JSValueRef arguments[],
                                            JSValueRef *exception) -> JSValueRef {
            return call(ctx, function, thisObject, argumentCount, arguments, exception);
        });
    }
    virtual Caller *get_static_caller() {
        return &overloaded_function::caller;
    }
    virtual int get_nargs() {
        return -1;
    }

  public:
    JSValueRef call(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount,
                    const JSValueRef arguments[], JSValueRef *exception) {
        return callers[argumentCount]->get_caller_impl()(ctx, function, thisObject, argumentCount, arguments,
                                                         exception);
    }

  public:
    static JSValueRef caller(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount,
                             const JSValueRef arguments[], JSValueRef *exception) {
        return JSValueMakeNull(ctx);
    }
};

} // namespace detail
} // namespace ulbind17
