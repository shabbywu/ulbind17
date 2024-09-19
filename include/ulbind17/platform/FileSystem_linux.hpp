#pragma once

#include <Ultralight/platform/FileSystem.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <ulbind17/mimetypes.hpp>

namespace ulbind17 {
namespace platform {
/**
 * FileSystem implementation for unix like.
 */
class FileSystem : public ultralight::FileSystem {
  protected:
    struct FileSystemBasic_BufferData {
        std::unique_ptr<std::ifstream> file_handle;
        int64_t file_size;
        std::unique_ptr<std::vector<char>> file_contents;
    };

    static void FileSystemBasic_DestroyBufferCallback(void *user_data, void *data) {
        FileSystemBasic_BufferData *buffer_data = reinterpret_cast<FileSystemBasic_BufferData *>(user_data);
        buffer_data->file_contents.reset();
        buffer_data->file_handle.reset();
        delete buffer_data;
    }

  public:
    FileSystem(std::filesystem::path rootdir) : rootdir(rootdir) {};
    virtual ~FileSystem() {};

    virtual bool FileExists(const ultralight::String &file_path) override {
        return std::filesystem::exists(rootdir / file_path.utf8().data());
    }

    virtual ultralight::String GetFileMimeType(const ultralight::String &file_path) override {
        std::filesystem::path path = file_path.utf8().data();
        std::string mimetype = mimetypes::getType(path.extension().string().c_str());
        return ultralight::String8(mimetype.c_str(), mimetype.length());
    }

    virtual ultralight::String GetFileCharset(const ultralight::String &file_path) override {
        return "utf-8";
    }

    virtual ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &file_path) override {
        std::filesystem::path path = file_path.utf8().data();

        std::unique_ptr<std::ifstream> file(new std::ifstream(path, std::ifstream::ate | std::ifstream::binary));
        if (!file->good())
            return nullptr;

        file->seekg(0, file->end);
        int64_t file_size = (int64_t)file->tellg();

        if (file_size <= 0)
            return nullptr;

        std::unique_ptr<std::vector<char>> buffer(new std::vector<char>(file_size));

        file->seekg(0, file->beg);
        file->read(buffer->data(), (std::streamsize)file_size);

        FileSystemBasic_BufferData *buffer_data = new FileSystemBasic_BufferData();
        buffer_data->file_handle = std::move(file);
        buffer_data->file_size = file_size;
        buffer_data->file_contents = std::move(buffer);

        return ultralight::Buffer::Create(buffer_data->file_contents->data(), buffer_data->file_size, buffer_data,
                                          FileSystemBasic_DestroyBufferCallback);
    }

  protected:
    std::filesystem::path rootdir;
};

} // namespace platform
} // namespace ulbind17
