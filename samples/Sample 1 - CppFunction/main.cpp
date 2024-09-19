#include <ulbind17/setup.hpp>
#include <ulbind17/ulbind17.hpp>

// the partial specialization of A is enabled via a template parameter

std::string vanilla() {
    return "vanilla c++ function is ok";
}

struct A {
    double i = 1.1;
    double add(int j) {
        i += j;
        return i;
    }
};

///
///  Welcome to Sample 1!
///
///  In this sample we'll show how to integrate C++ code with JavaScript.
///
///  We will introduce the DOMReady event and use it to bind some C++ callback to a JavaScript
///  function on JavaScrip context. Later, when that callback is triggered, we will execute a script
///  to call those c++ callback.
void Sample1() {
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
    window.set("lambda", 1);

    ulbind17::detail::generic_cast<int, JSValueRef>(ctx->ctx(), 1);
    ulbind17::detail::generic_cast<std::string, JSValueRef>(ctx->ctx(), "");

    window.bindFunc("logInfo", [](std::string message) { std::cout << message << std::endl; });
    window.bindFunc("lambda", [](std::string who) { return std::string("hello ") + who; });

    std::string secret = "secret can't access by javascript???";
    window.bindFunc("lambdaCapture", [&]() { return secret; });
    window.bindFunc("vanilla", &vanilla);
    A a;
    window.bindFunc("addJ", std::function<double(int)>(std::bind(&A::add, &a, std::placeholders::_1)));

    ulbind17::detail::Script script(ctx->ctx(), R"(
        logInfo("call lambda: " + lambda("world"))
        logInfo("call lambdaCapture: " + lambdaCapture())
        logInfo("call vanilla: " + vanilla())
        logInfo("call addJ: " + addJ(3))
    )");
    script.Evaluate<void>();
}

int main() {
    ulbind17::setup_ultralight_platform();
    Sample1();

    std::cin.get();
    return 0;
}
