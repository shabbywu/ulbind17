#include <ulbind17/setup.hpp>
#include <ulbind17/ulbind17.hpp>

// the partial specialization of A is enabled via a template parameter
///
///  Welcome to Sample 2!
///
///  In this sample we'll show how to integrate C++ code with JavaScript.
///
///  We will introduce the DOMReady event and use it to bind some C++ callback to a JavaScript
///  function on JavaScrip context. Later, when that callback is triggered, we will execute a script
///  to call those c++ callback.
void Sample2() {
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
    auto array = ulbind17::detail::Array(ctx->ctx());
    window.set("array", array);
    array.set(0, std::string("s0"));
    array.set(1, 1);
    array[2] = 2.2;
    array.set(4, ulbind17::detail::Undefined(ctx->ctx()));
    array.set(5, nullptr);
    array.set(6, true);
    window.bindFunc("logInfo", [](std::string message) { std::cout << message << std::endl; });

    ulbind17::detail::Script script(ctx->ctx(), R"(
        logInfo("" + array) // "s0,1,2.2,,s4"
        array[0] = 0
        logInfo("" + array) // "0,1,2.2,,s4"
    )");
    script.Evaluate<void>();
    assert(array.get<int>(0) == 0);
    std::cout << "array[0]: " << array.get<int>(0) << std::endl;
}

int main() {
    ulbind17::setup_ultralight_platform();
    Sample2();

    std::cin.get();
    return 0;
}
