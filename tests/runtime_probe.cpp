#include <ulbind17/runtime.hpp>
#ifdef _WIN32
#define PROBE_EXPORT __declspec(dllexport)
#else
#define PROBE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" PROBE_EXPORT int ulbind17_probe() {
    // Header-only runtime consumers need no embedded resource linkage.
    try { ulbind17::Runtime::FromExisting(nullptr); }
    catch (const ulbind17::Error &error) {
        return error.code == ulbind17::ErrorCode::InvalidArgument ? ultralight::String("probe").utf8().length() : 0;
    }
    return 0;
}
