#pragma once
#include <Ultralight/platform/FileSystem.h>
#include <filesystem>
#include <map>
#include <windows.h>

#include <ulbind17/mimetypes.hpp>

namespace ulbind17 {
namespace platform {
/**
 * FileSystem implementation for windows.
 */
class FileSystem : public ultralight::FileSystem {
  protected:
    struct FileSystemWin_BufferData {
        HANDLE hFile;
        HANDLE hMap;
        LPVOID lpBasePtr;
    };

    static void FileSystemWin_DestroyBufferCallback(void *user_data, void *data) {
        FileSystemWin_BufferData *buffer_data = reinterpret_cast<FileSystemWin_BufferData *>(user_data);
        UnmapViewOfFile(buffer_data->lpBasePtr);
        CloseHandle(buffer_data->hMap);
        CloseHandle(buffer_data->hFile);
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
        std::string mimetype = minetypes::getType(path.extension().string().c_str());
        return ultralight::String8(mimetype.c_str(), mimetype.length());
    }

    virtual ultralight::String GetFileCharset(const ultralight::String &file_path) override {
        return "utf-8";
    }

    virtual ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &file_path) override {
        auto fullpath = rootdir / file_path.utf8().data();
        HANDLE hFile;
        HANDLE hMap;
        LPVOID lpBasePtr;
        LARGE_INTEGER liFileSize;

        hFile = CreateFileW(fullpath.wstring().data(),
                            GENERIC_READ,          // dwDesiredAccess
                            FILE_SHARE_READ,       // dwShareMode
                            NULL,                  // lpSecurityAttributes
                            OPEN_EXISTING,         // dwCreationDisposition
                            FILE_ATTRIBUTE_NORMAL, // dwFlagsAndAttributes
                            0);                    // hTemplateFile

        if (hFile == INVALID_HANDLE_VALUE) {
            return nullptr;
        }

        if (!GetFileSizeEx(hFile, &liFileSize)) {
            CloseHandle(hFile);
            return nullptr;
        }

        if (liFileSize.QuadPart == 0) {
            CloseHandle(hFile);
            return nullptr;
        }

        hMap = CreateFileMapping(hFile,
                                 NULL,          // Mapping attributes
                                 PAGE_READONLY, // Protection flags
                                 0,             // MaximumSizeHigh
                                 0,             // MaximumSizeLow
                                 NULL);         // Name

        if (hMap == 0) {
            CloseHandle(hFile);
            return nullptr;
        }

        lpBasePtr = MapViewOfFile(hMap,
                                  FILE_MAP_READ, // dwDesiredAccess
                                  0,             // dwFileOffsetHigh
                                  0,             // dwFileOffsetLow
                                  0);            // dwNumberOfBytesToMap

        if (lpBasePtr == NULL) {
            CloseHandle(hMap);
            CloseHandle(hFile);
            return nullptr;
        }

        FileSystemWin_BufferData *buffer_data = new FileSystemWin_BufferData();
        buffer_data->hFile = hFile;
        buffer_data->hMap = hMap;
        buffer_data->lpBasePtr = lpBasePtr;

        return ultralight::Buffer::Create((char *)lpBasePtr, (size_t)liFileSize.QuadPart, buffer_data,
                                          FileSystemWin_DestroyBufferCallback);
    }

  protected:
    std::filesystem::path rootdir;
};

} // namespace platform
} // namespace ulbind17
