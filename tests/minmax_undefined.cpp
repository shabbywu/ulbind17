#include <ulbind17/mimetypes.hpp>

#if defined(min) || defined(max)
#error "Including mimetypes.hpp must not introduce min or max macros"
#endif

#include <cstring>

bool check_undefined_macros() {
    return std::strcmp(ulbind17::mimetypes::getType("image.png"), "image/png") == 0;
}
