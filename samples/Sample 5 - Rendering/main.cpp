#include "../common.hpp"

int showGUI(sample::RefPtr<sample::Renderer> &renderer, sample::RefPtr<sample::View> &view, bool smoke);

void Sample5(bool smoke) {
    sample::Fixture fixture;
    fixture.view->Resize(800, 450);
    int clicks = 0;
    ulbind17::js::API api("app");
    api["logInfo"] = [&clicks](std::string message) { ++clicks; std::cout << message << '\n'; };
    sample::check(api.AttachTo(fixture.view.get()), "AttachTo failed");
    fixture.view->LoadHTML(R"(
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

<button class="button button1" onclick="app.logInfo('click green');">Green</button>
<button class="button button2" onclick="app.logInfo('click Blue');">Blue</button>
<button class="button button3" onclick="app.logInfo('click Red');">Red</button>
<button class="button button4" onclick="app.logInfo('click Gray');">Gray</button>
<button class="button button5" onclick="app.logInfo('click Black');">Black</button>

</body>
</html>



    )");


    sample::check(showGUI(fixture.renderer, fixture.view, smoke) == 0, "GLFW rendering failed");
    if (smoke)
        sample::check(clicks == 1, "mouse click did not call the native API exactly once");
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && std::string(argv[1]) == "--smoke-test";
    return sample::run([&] { Sample5(smoke); });
}
