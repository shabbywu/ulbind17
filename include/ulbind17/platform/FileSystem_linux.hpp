#pragma once
#include <Ultralight/platform/FileSystem.h>
#include <filesystem>
#include <map>

namespace ulbind17 {
namespace platform {
/**
 * FileSystem implementation.
 */
class FileSystem : public ultralight::FileSystem {
  public:
    FileSystem(std::filesystem::path rootdir) : rootdir(rootdir) {};
    virtual ~FileSystem() {};

    virtual bool FileExists(const ultralight::String &file_path) override {
        return std::filesystem::exists(rootdir / file_path.utf8().data());
    }

    virtual ultralight::String GetFileMimeType(const ultralight::String &file_path) override {
    }

    virtual ultralight::String GetFileCharset(const ultralight::String &file_path) override {
        return "utf-8";
    }

    virtual ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &file_path) override {
    }

  protected:
    std::filesystem::path rootdir;
};

} // namespace platform
} // namespace ulbind17
