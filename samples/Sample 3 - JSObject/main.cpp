#include <ulbind17/setup.hpp>
#include <ulbind17/ulbind17.hpp>

// the partial specialization of A is enabled via a template parameter
///
///  Welcome to Sample 3!
///
///  In this sample we'll show how to integrate C++ code with JavaScript.
///
///  We will introduce the DOMReady event and use it to bind some C++ callback to a JavaScript
///  function on JavaScrip context. Later, when that callback is triggered, we will execute a script
///  to call those c++ callback.
void Sample3() {
    /// Create the Renderer
    RefPtr<Renderer> renderer = Renderer::Create();
    ViewConfig cfg;
    /// Create View
    auto view = renderer->CreateView(0, 0, cfg, nullptr);

    std::cout << std::endl;
    /// Finnaly get the js context
    auto ctx = view->LockJSContext();

    /// GlobalObject is also known as `window` in JavaScript
    auto window = ulbind17::detail::Object::GetGlobalObject(ctx->ctx());
    auto object = ulbind17::detail::Object(ctx->ctx());
    window.set("object", object);
    object.set(0, std::string("s0"));
    object.set(1, 1);
    object[2] = 2.2;
    object.set(4, ulbind17::detail::Undefined(ctx->ctx()));
    object.set(5, nullptr);
    object["6"] = true;
    object.bindFunc("logInfo", [](std::string message) { std::cout << message << std::endl; });
    window.bindFunc("logInfo", [](std::string message) { std::cout << message << std::endl; });

    std::cout << "Object.size: " << object.size() << std::endl;
    assert(object.size() == 7);

    ulbind17::detail::Script script(ctx->ctx(), R"(
        logInfo("" + object)
        object[0] = 0
        object.logInfo("also work")
        logInfo("" + object)
    )");
    script.Evaluate<void>();
    assert(object.get<int>(0) == 0);
    assert(object.get<bool>("6"));
    std::cout << "object[0]: " << object.get<int>("0") << std::endl;
}

int main() {
    ulbind17::setup_ultralight_platform();
    Sample3();

    std::cin.get();
    return 0;
}
