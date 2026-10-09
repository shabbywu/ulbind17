#include "../common.hpp"
#include <vector>

void Sample2() {
    sample::Fixture fixture;
    ulbind17::js::Context context(fixture.view.get());
    auto array = context.MakeArray({});
    array[0] = std::string("s0");
    array[1] = 1;
    array[2] = 2.2;
    // Index 3 remains a hole; index 4 exists and contains undefined.
    array[4] = ulbind17::js::undefined;
    array[5] = ulbind17::js::null;
    array[6] = true;
    sample::take(context.GlobalObject().SetProperty("array", array));

    sample::check(sample::take(ulbind17::size(array)) == 7, "array length must include holes");
    sample::check(!array.Has("3") && array.Has("4"), "hole and undefined must remain distinct");
    sample::check(sample::take(ulbind17::get<ulbind17::js::Value>(array, 4)).IsUndefined(), "undefined lost");
    sample::check(sample::take(ulbind17::get<ulbind17::js::Value>(array, 5)).IsNull(), "null lost");
    sample::check(sample::take(ulbind17::get<bool>(array, 6)), "boolean lost");
    sample::check(sample::take(ulbind17::keys(array)) ==
                      std::vector<std::string>({"0", "1", "2", "4", "5", "6"}),
                  "array key enumeration failed");

    auto iterator = sample::take(array["keys"].Invoke<ulbind17::js::Value>());
    auto first = sample::take(iterator["next"].Invoke<ulbind17::js::Value>());
    auto second = sample::take(iterator["next"].Invoke<ulbind17::js::Value>());
    sample::check(sample::take(ulbind17::get<int>(first, "value")) == 0 &&
                      sample::take(ulbind17::get<int>(second, "value")) == 1,
                  "iterator failed");
    sample::take(context.Evaluate("array[0] = 0"));
    sample::check(sample::take(ulbind17::get<int>(array, 0)) == 0, "page mutation was not observed");
    std::cout << "array length: " << sample::take(ulbind17::size(array)) << '\n';
}

int main() {
    return sample::run(Sample2);
}
