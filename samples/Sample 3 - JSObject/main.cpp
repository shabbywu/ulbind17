#include "../common.hpp"

void Sample3() {
    sample::Fixture fixture;
    ulbind17::js::Context context(fixture.view.get());
    auto object = context.MakeObject();
    object[0] = std::string("s0");
    object[1] = 1;
    object[2] = 2.2;
    object[4] = ulbind17::js::undefined;
    object[5] = ulbind17::js::null;
    object["六"] = true;
    object["logInfo"] = context.MakeFunction("logInfo", [](std::string message) { std::cout << message << '\n'; });
    sample::take(context.GlobalObject().SetProperty("object", object));

    sample::check(sample::take(ulbind17::size(object)) == 7, "object size failed");
    sample::check(sample::take(ulbind17::get<bool>(object, "六")), "UTF-8 key failed");
    sample::take(context.Evaluate("object[0] = 0; object.logInfo('native object callback')"));
    sample::check(sample::take(ulbind17::get<int>(object, "0")) == 0, "page mutation was not observed");
    for (const auto &key : sample::take(ulbind17::keys(object)))
        std::cout << key << ' ';
    std::cout << '\n';
}

int main() {
    return sample::run(Sample3);
}
