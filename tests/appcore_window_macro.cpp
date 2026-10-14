// Check the Windows-header-first order even on non-Windows builders.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#define CreateWindowA(...) CreateWindowExA(0, __VA_ARGS__)
#define CreateWindow CreateWindowA
#endif
#include <ulbind17/appcore.hpp>

#ifndef CreateWindow
#error "AppCore adapter must preserve the caller's Win32 macro"
#endif

static_assert(requires(const ulbind17::appcore::Application &app) { app.NewWindow(100, 100); });

// Legacy callers can opt out of the Win32 alias locally.
#undef CreateWindow
static_assert(requires(const ulbind17::appcore::Application &app) { app.CreateWindow(100, 100); });
