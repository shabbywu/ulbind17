#pragma once
#include <ulbind17/platform/FileSystem.hpp>
#include <filesystem>
#include <utility>

namespace ulbind17::resources::filesystem {
// Reads only the two SDK resource names under the configured resource prefix.
// This backend needs ulbind17::header, without embedded_resources.
class SDKResources {
    std::filesystem::path rootdir_, resource_dir_;
    platform::FileSystem files_;

    bool accepts(const std::filesystem::path &path) const {
        return path == (resource_dir_ / "icudt67l.dat").lexically_normal() ||
               path == (resource_dir_ / "cacert.pem").lexically_normal();
    }

  public:
    explicit SDKResources(std::filesystem::path rootdir = ".", std::filesystem::path resource_dir = "resources/")
        : rootdir_(std::move(rootdir)), resource_dir_(std::move(resource_dir)), files_(rootdir_) {}

    bool FileExists(const ultralight::String &name) const {
        const auto path = std::filesystem::path(name.utf8().data()).lexically_normal();
        std::error_code error;
        return accepts(path) && std::filesystem::is_regular_file(rootdir_ / path, error);
    }

    ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &name) {
        if (!FileExists(name)) return nullptr;
        const auto path = std::filesystem::path(name.utf8().data()).lexically_normal().generic_string();
        return files_.OpenFile(path.c_str());
    }
};
} // namespace ulbind17::resources::filesystem
