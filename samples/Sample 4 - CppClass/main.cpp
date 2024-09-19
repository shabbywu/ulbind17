#include <ulbind17/setup.hpp>
#include <ulbind17/ulbind17.hpp>

class A {
  public:
    int i;
    A() {
        i = 0;
    }

    double method(int time, std::string message) {
        for (int i = 0; i < time; i++) {
            std::cout << "message: " << message << std::endl;
        }
        return i * 2.1;
    }
};

// the partial specialization of A is enabled via a template parameter
///
///  Welcome to Sample 4!
///
///  In this sample we'll show how to integrate C++ code with JavaScript.
///
///  We will introduce the DOMReady event and use it to bind some C++ callback to a JavaScript
///  function on JavaScrip context. Later, when that callback is triggered, we will execute a script
///  to call those c++ callback.
void Sample4() {
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

    // TODO:
    // 1. create an jsobject as class constructor
    // 2. support to defined constructor at cpp
    auto clazz =
        ulbind17::detail::ClassDef<A>(ctx->ctx(), "A").defProperty("i", &A::i).bindFunc("method", &A::method).end();
    A a;
    window.set("a", &a);
    // window.set("A", clazz->rawref());
    window.set("logInfo", [](std::string message) { std::cout << message << std::endl; });
    ulbind17::detail::Script(ctx->ctx(), R"(
        logInfo("a.i = " + a.i)
        a.i++
        a.method(3, "重要的事情说三遍");
        logInfo(typeof logInfo)
    )")
        .Evaluate();
    JSGarbageCollect(ctx->ctx());
    std::cout << "after js\n> a.i = " << a.i << std::endl;
}

int main() {
    ulbind17::setup_ultralight_platform();
    Sample4();

    std::cin.get();
    return 0;
}
