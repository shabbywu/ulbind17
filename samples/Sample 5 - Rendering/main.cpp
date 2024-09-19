#include <Ultralight/Ultralight.h>
#include <ulbind17/setup.hpp>
#include <ulbind17/ulbind17.hpp>

int showGUI(RefPtr<Renderer> &render, RefPtr<View> &view);

// the partial specialization of A is enabled via a template parameter
///
///  Welcome to Sample 5!
///
///  In this sample we'll show how to integrate C++ code with JavaScript.
///
///  We will introduce the DOMReady event and use it to bind some C++ callback to a JavaScript
///  function on JavaScrip context. Later, when that callback is triggered, we will execute a script
///  to call those c++ callback.
void Sample5() {
    /// Create the Renderer
    RefPtr<Renderer> renderer = Renderer::Create();
    ViewConfig cfg;
    cfg.is_accelerated = false;
    /// Create View
    auto view = renderer->CreateView(800, 450, cfg, nullptr);
    view->LoadHTML(R"(
<!DOCTYPE html>
<html>
<head>
<style>
.button {
  background-color: #04AA6D; /* Green */
  border: none;
  color: white;
  padding: 16px 32px;
  text-align: center;
  text-decoration: none;
  display: inline-block;
  font-size: 16px;
  margin: 4px 2px;
  transition-duration: 0.4s;
  cursor: pointer;
}

.button1 {
  background-color: white;
  color: black;
  border: 2px solid #04AA6D;
}

.button1:hover {
  background-color: #04AA6D;
  color: white;
}

.button2 {
  background-color: white;
  color: black;
  border: 2px solid #008CBA;
}

.button2:hover {
  background-color: #008CBA;
  color: white;
}

.button3 {
  background-color: white;
  color: black;
  border: 2px solid #f44336;
}

.button3:hover {
  background-color: #f44336;
  color: white;
}

.button4 {
  background-color: white;
  color: black;
  border: 2px solid #e7e7e7;
}

.button4:hover {background-color: #e7e7e7;}

.button5 {
  background-color: white;
  color: black;
  border: 2px solid #555555;
}

.button5:hover {
  background-color: #555555;
  color: white;
}
</style>
</head>
<body>

<h2>Hoverable Buttons</h2>

<p>Use the :hover selector to change the style of the button when you move the mouse over it.</p>
<p><strong>Tip:</strong> Use the transition-duration property to determine the speed of the "hover" effect:</p>

<button class="button button1" onclick="logInfo('click green');">Green</button>
<button class="button button2" onclick="logInfo('click Blue');">Blue</button>
<button class="button button3" onclick="logInfo('click Red');">Red</button>
<button class="button button4" onclick="logInfo('click Gray');">Gray</button>
<button class="button button5" onclick="logInfo('click Black');">Black</button>

</body>
</html>



    )");

    std::cout << std::endl;
    /// Finnaly get the js context
    auto ctx = view->LockJSContext();

    /// GlobalObject is also known as `window` in JavaScript
    auto window = ulbind17::detail::Object::GetGlobalObject(ctx->ctx());

    // TODO:
    // 1. create an jsobject as class constructor
    // 2. support to defined constructor at cpp

    window.bindFunc("logInfo", [](std::string message) { std::cout << message << std::endl; });

    try {
        showGUI(renderer, view);
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

int main() {
    ulbind17::setup_ultralight_platform();
    Sample5();

    std::cin.get();
    return 0;
}
