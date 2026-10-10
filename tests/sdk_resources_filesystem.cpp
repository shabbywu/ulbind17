#include <ulbind17/resources/filesystem/SDKResources.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main() {
    ulbind17::resources::filesystem::SDKResources resources(ULBIND17_SDK_RESOURCES, ".");
    if (resources.FileExists("resources/cacert.pem") || resources.OpenFile("unknown.dat").get()) {
        std::cerr << "Filesystem resource lookup accepted an unknown name or wrong prefix\n";
        return 1;
    }
    for (const char *name : {"icudt67l.dat", "cacert.pem"}) {
        const std::string requested = std::string("./unused/../") + name;
        if (!resources.FileExists(requested.c_str())) return 2;
        auto buffer = resources.OpenFile(requested.c_str());
        std::ifstream input(std::filesystem::path(ULBIND17_SDK_RESOURCES) / name, std::ios::binary);
        const std::vector<char> expected((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (!buffer.get() || expected.empty() || buffer->size() != expected.size() ||
            !std::equal(expected.begin(), expected.end(), static_cast<const char *>(buffer->data()))) {
            std::cerr << "Filesystem resource differs from the selected SDK: " << name << '\n';
            return 3;
        }
    }
    ultralight::RefPtr<ultralight::Buffer> retained;
    {
        ulbind17::resources::filesystem::SDKResources temporary(ULBIND17_SDK_RESOURCES, ".");
        retained = temporary.OpenFile("cacert.pem");
    }
    if (!retained.get() || retained->size() == 0 || !retained->data()) return 4;
    std::ifstream certificate(std::filesystem::path(ULBIND17_SDK_RESOURCES) / "cacert.pem", std::ios::binary);
    const auto first_byte = certificate.get();
    if (first_byte < 0 || static_cast<const unsigned char *>(retained->data())[0] != first_byte) return 4;
    std::cout << "Filesystem SDK resources PASS\n";
}
