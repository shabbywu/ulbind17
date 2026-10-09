// Model the function-like macros exported by windows.h, including on other hosts.
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#include <ulbind17/mimetypes.hpp>

static_assert(min(7, 3) == 3, "Including mimetypes.hpp must preserve min");
static_assert(max(7, 3) == 7, "Including mimetypes.hpp must preserve max");

#undef min
#undef max
#include <cstring>
#include <limits>
static_assert(std::numeric_limits<int>::min() < 0);
static_assert(std::numeric_limits<int>::max() > 0);

bool check_undefined_macros();

int main() {
    return std::strcmp(ulbind17::mimetypes::getType("index.html"), "text/html") != 0 ||
           !check_undefined_macros();
}
