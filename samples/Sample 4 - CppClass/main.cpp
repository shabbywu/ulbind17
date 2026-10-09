#include "../common.hpp"

class A {
  public:
    int i = 0;

    double method(int times, std::string message) {
        for (int count = 0; count < times; ++count)
            std::cout << message << '\n';
        return i * 2.1;
    }
};

void Sample4() {
    sample::Fixture fixture;
    A a;
    ulbind17::js::API api("app");
    api.DefineClass<A>("A").Constructor<>().Field("i", &A::i).Method("method", &A::method);
    // Borrowed instance: a must outlive every page call, or its wrappers must be detached.
    api.BindProperty("a", [&a]() { return &a; });
    sample::check(api.AttachTo(fixture.view.get()), "AttachTo failed");
    ulbind17::js::Context context(fixture.view.get());
    auto result = context.Evaluate<double>("app.a.i++; app.a.method(3, '重要的事情说三遍')");
    sample::check(sample::take(std::move(result)) == 2.1 && a.i == 1, "borrowed class binding failed");
    sample::check(sample::take(context.Evaluate<int>("const owned = new app.A(); owned.i = 7; owned.i")) == 7,
                  "native constructor failed");
    auto borrowed = context.Make(&a);
    sample::take(context.GlobalObject().SetProperty("borrowed", borrowed));
    ulbind17::js::Detach<A>(borrowed);
    auto detached = context.Evaluate("borrowed.method(1, 'detached')");
    sample::check(!detached && detached.error().code() == "ULJS_DETACHED", "detached instance must fail");
}

int main() {
    return sample::run(Sample4);
}
