#include <filesystem>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
int main(int argc, char **argv) {
    if (argc != 2) return 1;
    const auto parent = std::filesystem::path(argv[1]).parent_path();
    for (auto name : {"AppCore.dll", "libAppCore.dylib", "libAppCore.so"})
        if (std::filesystem::exists(parent / name)) return 2;
#ifdef _WIN32
    // Keep dependency resolution local to the explicitly loaded module.
    auto library = LoadLibraryExW(std::filesystem::path(argv[1]).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!library) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 3; }
    auto probe = reinterpret_cast<int (*)()>(GetProcAddress(library, "ulbind17_probe"));
    auto result = probe && probe() == 5;
    FreeLibrary(library);
#else
    auto library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::cerr << dlerror() << '\n'; return 3; }
    auto probe = reinterpret_cast<int (*)()>(dlsym(library, "ulbind17_probe"));
    auto result = probe && probe() == 5;
    dlclose(library);
#endif
    return result ? 0 : 4;
}
