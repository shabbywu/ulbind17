#include <ulbind17/resources/FileSystemOverlay.hpp>
#include <ulbind17/resources/filesystem/SDKResources.hpp>
#ifdef ULBIND17_TEST_EMBEDDED
#include <ulbind17/resources/embedded/SDKResources.hpp>
#endif
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

struct HostFiles final : ultralight::FileSystem {
    std::atomic<int> opens = 0;
    bool FileExists(const ultralight::String &) override { return true; }
    ultralight::String GetFileMimeType(const ultralight::String &) override { return "host/type"; }
    ultralight::String GetFileCharset(const ultralight::String &) override { return "host/charset"; }
    ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &) override {
        ++opens; return ultralight::Buffer::CreateFromCopy("host", 4);
    }
};
int main() {
    auto host = std::make_shared<HostFiles>();
#ifdef ULBIND17_TEST_EMBEDDED
    auto provider = ulbind17::resources::embedded::SDKResources(".");
#else
    auto provider = ulbind17::resources::filesystem::SDKResources(ULBIND17_SDK_RESOURCES, ".");
#endif
    ulbind17::resources::FileSystemOverlay overlay(std::move(provider), host);
    auto resource = overlay.OpenFile("cacert.pem");
    if (!resource.get() || resource->size() <= 4 || host->opens != 0) return 1;
    if (!overlay.FileExists("page.html") || overlay.OpenFile("page.html")->size() != 4) return 2;
    if (std::string(overlay.GetFileMimeType("page.html").utf8().data()) != "host/type" ||
        std::string(overlay.GetFileCharset("page.html").utf8().data()) != "host/charset") return 3;
    std::atomic<bool> failed = false;
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) readers.emplace_back([&] {
        for (int j = 0; j < 10; ++j) {
            auto buffer = overlay.OpenFile("cacert.pem");
            if (!buffer.get() || buffer->size() != resource->size()) failed = true;
        }
    });
    for (auto &reader : readers) reader.join();
    if (failed || host->opens != 1) return 4;
    // Missing resource data still falls back to the host.
    ulbind17::resources::FileSystemOverlay missing(
        ulbind17::resources::filesystem::SDKResources("ulbind17-nonexistent", "."), host);
    if (missing.OpenFile("cacert.pem")->size() != 4) return 5;
    std::cout << "Resource overlay PASS\n";
}
