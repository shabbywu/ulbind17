#include "../common.hpp"
#include <functional>

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

void Sample1() {
    sample::Fixture fixture;
    std::string secret = "native capture";
    A a;
    ulbind17::js::API api("app");
    api["logInfo"] = [](std::string message) { std::cout << message << '\n'; };
    api["hello"] = [](std::string who) { return std::string("hello ") + who; };
    api["capture"] = [&secret]() { return secret; };
    api["vanilla"] = &vanilla;
    api["addJ"] = ulbind17::js::Bind(&a, &A::add);
    api["function"] = std::function<int(int)>([](int value) { return value * 2; });
    sample::check(api.AttachTo(fixture.view.get()), "AttachTo failed");

    ulbind17::js::Context context(fixture.view.get());
    sample::check(sample::take(context.Evaluate<std::string>("app.hello('world')")) == "hello world",
                  "lambda binding failed");
    sample::check(sample::take(context.Evaluate<std::string>("app.capture()")) == secret,
                  "capture binding failed");
    sample::check(sample::take(context.Evaluate<std::string>("app.vanilla()")) == vanilla(),
                  "free function binding failed");
    sample::check(sample::take(context.Evaluate<double>("app.addJ(3)")) == 4.1, "member binding failed");
    sample::check(sample::take(context.Evaluate<int>("app.function(4)")) == 8, "std::function binding failed");
    auto bad = context.Evaluate("app.addJ('three')");
    sample::check(!bad && bad.error().code() == "ULJS_BAD_ARG", "wrong arguments must fail");
    sample::take(context.Evaluate("app.logInfo(app.hello('world'))"));
}

int main() {
    return sample::run(Sample1);
}
