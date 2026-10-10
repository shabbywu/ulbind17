#pragma once
#include <Ultralight/Buffer.h>
#include <Ultralight/String.h>
#include <ulbind17/resources/sdk_resources.hpp>
#include <filesystem>
#include <utility>

namespace ulbind17::resources::embedded {
// Resolves SDK resources from static data only; never opens host files.
class SDKResources {
    struct Resource {
        const unsigned char *data = nullptr;
        std::size_t size = 0;
    };
    std::filesystem::path resource_dir_;

    Resource find(const ultralight::String &name) const {
        const auto path = std::filesystem::path(name.utf8().data()).lexically_normal();
        if (path == (resource_dir_ / "icudt67l.dat").lexically_normal())
            return {icudt67l_data, icudt67l_size};
        if (path == (resource_dir_ / "cacert.pem").lexically_normal())
            return {cacert_data, cacert_size};
        return {};
    }

  public:
    explicit SDKResources(std::filesystem::path resource_dir = "resources/")
        : resource_dir_(std::move(resource_dir)) {}

    bool FileExists(const ultralight::String &name) const { return find(name).data != nullptr; }

    ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &name) const {
        const auto resource = find(name);
        return resource.data
            ? ultralight::Buffer::Create(const_cast<unsigned char *>(resource.data), resource.size, nullptr, nullptr)
            : nullptr;
    }
};
} // namespace ulbind17::resources::embedded
