#pragma once
#include <CoreFoundation/CFString.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>
#include <Ultralight/platform/FileSystem.h>
#include <filesystem>
#include <map>

namespace ulbind17 {
namespace platform {
/**
 * FileSystem implementation for MacOS.
 */
class FileSystem : public ultralight::FileSystem {
  protected:
    static NSString *ToNSString(const ultralight::String16 &str) {
        auto pathData =
            [[NSData alloc] initWithBytesNoCopy:const_cast<void *>(reinterpret_cast<const void *>(str.data()))
                                         length:str.length() * sizeof(Char16)
                                   freeWhenDone:NO];
        return [[NSString alloc] initWithData:pathData encoding:NSUTF16LittleEndianStringEncoding];
    }
    ultralight::String16 ToString16(CFStringRef str) {
        if (!str)
            return ultralight::String16();
        CFIndex size = CFStringGetLength(str);
        std::vector<Char16> buffer(size);
        CFStringGetCharacters(str, CFRangeMake(0, size), (UniChar *)buffer.data());
        return ultralight::String16(buffer.data(), size);
    }
    struct FileSystemMac_BufferData {
        int file_handle;
        size_t file_size;
        void *file_map;
    };

    void FileSystemMac_DestroyBufferCallback(void *user_data, void *data) {
        FileSystemMac_BufferData *buffer_data = reinterpret_cast<FileSystemMac_BufferData *>(user_data);
        munmap(buffer_data->file_map, buffer_data->file_size);
        close(buffer_data->file_handle);
        delete buffer_data;
    }

  public:
    FileSystem(std::filesystem::path rootdir) : rootdir(rootdir) {};
    virtual ~FileSystem() {};

    virtual bool FileExists(const ultralight::String &file_path) override {
        return std::filesystem::exists(rootdir / file_path.utf8().data());
    }

    virtual ultralight::String GetFileMimeType(const ultralight::String &file_path) override {
        auto pathStr = ToNSString(getRelative(file_path.utf16()));
        CFStringRef extension = (__bridge CFStringRef)[pathStr pathExtension];
        CFStringRef mime_type = NULL;
        CFStringRef identifier = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension, NULL);
        if (identifier)
            mime_type = UTTypeCopyPreferredTagWithClass(identifier, kUTTagClassMIMEType);
        CFRelease(identifier);
        return ToString16(mime_type);
    }

    virtual ultralight::String GetFileCharset(const ultralight::String &file_path) override {
        return "utf-8";
    }

    virtual ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &file_path) override {
        auto fullpath = rootdir / file_path.utf8().data();
        int file_handle = open(fullpath.string().data(), O_RDONLY);

        if (file_handle == -1) {
            // Error: Could not open file for reading
            return nullptr;
        }

        struct stat file_info = {0};

        if (fstat(file_handle, &file_info) == -1) {
            // Error: Could not get the file size
            close(file_handle);
            return nullptr;
        }

        size_t file_size = file_info.st_size;

        if (file_size == 0) {
            // Error: File is empty
            close(file_handle);
            return nullptr;
        }

        void *file_map = mmap(0, file_size, PROT_READ, MAP_SHARED, file_handle, 0);

        if (file_map == MAP_FAILED) {
            // Error: File mapping failed
            close(file_handle);
            return nullptr;
        }

        FileSystemMac_BufferData *buffer_data = new FileSystemMac_BufferData();
        buffer_data->file_handle = file_handle;
        buffer_data->file_size = file_size;
        buffer_data->file_map = file_map;

        return ultralight::Buffer::Create(file_map, file_size, buffer_data, FileSystemMac_DestroyBufferCallback);
    }

  protected:
    std::filesystem::path rootdir;
};

} // namespace platform
} // namespace ulbind17
