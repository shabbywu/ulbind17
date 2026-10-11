#pragma once
#include <Ultralight/platform/FileSystem.h>
#include <memory>
#include <stdexcept>
#include <utility>

namespace ulbind17::resources {
// Provider implements FileExists/OpenFile (eg either SDKResources backend).
// The provider and fallback must support concurrent SDK file-loading calls.
template <typename Provider> class FileSystemOverlay final : public ultralight::FileSystem {
    Provider resources_;
    std::shared_ptr<ultralight::FileSystem> fallback_;
  public:
    FileSystemOverlay(Provider resources, std::shared_ptr<ultralight::FileSystem> fallback)
        : resources_(std::move(resources)), fallback_(std::move(fallback)) {
        if (!fallback_) throw std::invalid_argument("A resource overlay requires a host filesystem");
    }
    bool FileExists(const ultralight::String &path) override {
        return resources_.FileExists(path) || fallback_->FileExists(path);
    }
    ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &path) override {
        auto buffer = resources_.OpenFile(path);
        return buffer.get() ? buffer : fallback_->OpenFile(path);
    }
    ultralight::String GetFileMimeType(const ultralight::String &path) override {
        return fallback_->GetFileMimeType(path);
    }
    ultralight::String GetFileCharset(const ultralight::String &path) override {
        return fallback_->GetFileCharset(path);
    }
};
} // namespace ulbind17::resources
