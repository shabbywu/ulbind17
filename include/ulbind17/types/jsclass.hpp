#pragma once
#include "jsvalue.hpp"
#include "ulbind17/detail/malloc.hpp"
#include "ulbind17/types/jsstring.hpp"
#include "ulbind17/types/nativefunction.hpp"
#include <JavaScriptCore/JSRetainPtr.h>
#include <format>
#include <map>
#include <memory>
#include <set>

namespace ulbind17 {
namespace detail {

class ClassBase {
  public:
    virtual void end(JSClassDefinition &def) = 0;
    virtual JSObjectRef makeInstance(JSContextRef ctx, void *instance) = 0;
    virtual void initializeInstance(JSContextRef ctx, JSObjectRef object) = 0;
    virtual void finalizeInstance(JSObjectRef object) = 0;

    static void FinalizeExPrivateDataHolder(JSClassRef jsClass, JSObjectRef object) {
        auto cp = (PrivateDataHolder<ClassBase> *)JSClassGetPrivate(jsClass);
        auto op = (PrivateDataHolderFinalizer *)JSObjectGetPrivate(object);

        if (cp) {
            // cp->data->
        }
        if (op) {
            delete op;
        }
    }
};

template <typename C> class Class : public ClassBase {
  protected:
    using Holder = Holder<JSClassRef>;
    // getter = JSValueRef(this, ctx, exception*)
    using PropertyGetter = JSValueRef(C *, JSContextRef, JSValueRef *);
    // setter = bool(this, ctx, value, exception*)
    using PropertySetter = bool(C *, JSContextRef, JSValueRef, JSValueRef *);

    JSClassDefinition def;
    std::shared_ptr<Holder> holder;
    std::map<std::string, std::function<PropertyGetter>> getter;
    std::map<std::string, std::function<PropertySetter>> setter;
    std::set<std::shared_ptr<C>> instances;
    bool defined;

  public:
    Class(JSContextRef ctx, JSClassRef ref)
        : holder(std::make_shared<Holder>(ctx, ref)), defined(true) {

          };
    Class(JSContextRef ctx) : holder(std::make_shared<Holder>(ctx, nullptr)), defined(false) {};

  public:
    JSClassRef &rawref() const {
        return holder->value;
    }

    virtual void end(JSClassDefinition &def) {
        if (defined) {
            throw std::exception("class is defined");
        }
        defined = true;
        holder->value = JSClassCreate(&def);
        JSClassRetain(holder->value);
    }

    virtual JSObjectRef makeInstance(JSContextRef ctx, void *self) {
        return JSObjectMake(ctx, holder->value, self);
    }

    virtual void initializeInstance(JSContextRef ctx, JSObjectRef object) {
        // TODO: set by ClassDef
        // auto self = std::shared_ptr<C>();
        // instances.insert(self);
        // JSObjectSetPrivate(object, self.get());
    }

    virtual void finalizeInstance(JSObjectRef object) {
        C *p = (C *)JSObjectGetPrivate(object);
        auto it = findInstance(p);
        if (it != instances.end()) {
            instances.erase(it);
            delete p;
        }
    }

    JSValueRef getProperty(JSContextRef ctx, JSObjectRef object, std::string propertyName, JSValueRef *exception) {
        auto instance = (C *)JSObjectGetPrivate(object);
        auto i = getter.find(propertyName);
        if (i == getter.end()) {
            if (exception) {
                auto s = adopt(JSStringCreateWithUTF8CString((propertyName + " not found").c_str()));
                *exception = JSValueMakeString(ctx, s.get());
            }
            return Null(ctx).rawref();
        }
        return i->second(instance, ctx, exception);
    }

    bool setProperty(JSContextRef ctx, JSObjectRef object, std::string propertyName, JSValueRef value,
                     JSValueRef *exception) {
        auto instance = (C *)JSObjectGetPrivate(object);
        auto i = setter.find(propertyName);
        if (i == setter.end()) {
            if (exception) {
                auto s = adopt(JSStringCreateWithUTF8CString((propertyName + " not found").c_str()));
                *exception = JSValueMakeString(ctx, s.get());
            }
            return Null(ctx).rawref();
        }
        return i->second(instance, ctx, value, exception);
    }

  protected:
    auto findInstance(C *instance) {
        for (auto it = instances.begin(); it != instances.end(); it++) {
            if ((*it).get() == instance) {
                return it;
            }
        }
        return instances.end();
    }

  public:
    Class &operator=(const Class &other) {
        this->holder = other.holder;
        return *this;
    }

    template <typename C, typename P> void defProperty(std::string propertyName, P C::*pm) {
        std::function<PropertyGetter> fget = [=](C *self, JSContextRef ctx, JSValueRef *exception) {
            P v = self->*pm;
            return detail::generic_cast<P, JSValueRef>(ctx, std::forward<P>(v));
        };
        std::function<PropertySetter> fset = [=](C *self, JSContextRef ctx, JSValueRef value, JSValueRef *exception) {
            self->*pm = detail::generic_cast<JSValueRef, P>(ctx, std::forward<JSValueRef>(value));
            return true;
        };
        getter[propertyName] = fget;
        setter[propertyName] = fset;
    }

    template <typename Func> void bindFunc(std::string propertyName, Func &&func) {
        auto container = to_cpp_function(std::forward<Func>(func));
        JSObjectRef callback = NativeFunction<detail::function_signature_t<Func>>(holder->ctx, container).rawref();
        std::function<PropertyGetter> fget = [=](C *self, JSContextRef ctx, JSValueRef *exception) { return callback; };
        getter[propertyName] = fget;
    }
};

class ClassRegistry {
  public:
    std::map<size_t, std::shared_ptr<ClassBase>> classes;

    static ClassRegistry &getIntance() {
        static ClassRegistry instance;
        return instance;
    }

  public:
    template <class C> Class<C> *findJSClass() {
        size_t k = typeid(C).hash_code();
        auto i = classes.find(k);
        if (i == classes.end()) {
            return nullptr;
        } else {
            auto p = i->second;
            return dynamic_cast<Class<C> *>(p.get());
        }
    }
    template <class C> void registerJSClass(std::shared_ptr<ClassBase> item) {
        size_t k = typeid(C).hash_code();
        classes[k] = item;
    }
};

template <class C, class Base = void> class ClassDef {
  public:
    JSClassDefinition def;
    std::shared_ptr<Class<C>> clazz;

    ClassDef(JSContextRef ctx, const char *name) : clazz(std::make_shared<Class<C>>(ctx)) {
        memset(&def, 0, sizeof(def));
        def.className = name;
        def.version = 0;
        def.attributes = kJSClassAttributeNone;
        def.getProperty = [](JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef *exception) {
            auto clazz = ClassRegistry::getIntance().findJSClass<C>();
            auto name = String(ctx, propertyName).value();
            return clazz->getProperty(ctx, object, name, exception);
        };
        def.setProperty = [](JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value,
                             JSValueRef *exception) {
            auto clazz = ClassRegistry::getIntance().findJSClass<C>();
            auto name = String(ctx, propertyName).value();
            return clazz->setProperty(ctx, object, name, value, exception);
        };
        def.finalize = [](JSObjectRef object) {
            auto clazz = ClassRegistry::getIntance().findJSClass<C>();
            clazz->finalizeInstance(object);
        };
        def.initialize = [](JSContextRef ctx, JSObjectRef object) {
            auto clazz = ClassRegistry::getIntance().findJSClass<C>();
            clazz->initializeInstance(ctx, object);
        };

        auto base = ClassRegistry::getIntance().findJSClass<Base>();
        if (base != nullptr) {
            def.parentClass = base->rawref();
        }
        ClassRegistry::getIntance().registerJSClass<C>(clazz);
    };

    /// @brief End the Class Definition
    /// @return Class<C>
    std::shared_ptr<Class<C>> end() {
        clazz->end(def);
        return clazz;
    }

  public:
    template <typename P> ClassDef &defProperty(std::string propertyName, P C::*pm) {
        if (clazz) {
            clazz->defProperty(propertyName, pm);
        }
        return *this;
    }

  public:
    template <typename Func> ClassDef &bindFunc(std::string propertyName, Func &&func) {
        if (clazz) {
            clazz->bindFunc(propertyName, std::forward<Func>(func));
        }
        return *this;
    }
};

/// @brief A Proxy of Class<C>, can export to javascript context.
class ClassProxy {
  public:
    using Holder = Holder<JSObjectRef>;
    // template <typename C> static JSClassDefinition make_class_def() {
    //     auto clazz = ClassRegistry::getIntance().findJSClass<C>();
    //     JSClassDefinition def;
    //     memset(&def, 0, sizeof(def));
    //     def.className = clazz->def.name;
    //     def.attributes = kJSClassAttributeNone;
    //     def.callAsFunction = func->get_static_caller();
    //     def.finalize = &FinalizePrivateDataHolder;

    //     return def;
    // }

    // static JSClassRef make_class(std::shared_ptr<generic_function> func) {
    //     // dynamic data is store at class instance.
    //     // no matter what func it is, those classes are all same.
    //     static JSClassRef clazz;
    //     if (!clazz) {
    //         JSClassDefinition def = make_class_def(func);
    //         clazz = JSClassCreate(&def);
    //     }
    //     return clazz;
    // }

    // static JSObjectRef make_instance(JSContextRef ctx, std::shared_ptr<generic_function> func) {
    //     auto clazz = make_class(func);
    //     auto instance = JSObjectMake(ctx, clazz, new PrivateDataHolder<generic_function>(func));
    //     return instance;
    // }
};

} // namespace detail
} // namespace ulbind17
