#pragma once

#include "fonts/AlimamaShuHeiTi-Bold.h"
#include "fonts/FreeUniversal-Regular.h"
#include <ulbind17/resources/embedded/SDKResources.hpp>
#include <Ultralight/Ultralight.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <ulbind17/platform/FileSystem.hpp>
#include <ulbind17/platform/FontLoader.hpp>

namespace ulbind17 {
class StdoutLogger : public ultralight::Logger {
  public:
    void LogMessage(ultralight::LogLevel, const ultralight::String &message) override {
        std::cout << message.utf8().data() << std::endl;
    }

    static StdoutLogger *instance() {
        static StdoutLogger logger;
        return &logger;
    }
};

class EmbeddedResourceFileSystem : public platform::FileSystem {
  public:
    explicit EmbeddedResourceFileSystem(std::filesystem::path rootdir,
                                        std::filesystem::path resource_dir = "resources/")
        : platform::FileSystem(std::move(rootdir)), sdk_resources_(std::move(resource_dir)) {}

    bool FileExists(const ultralight::String &file_path) override {
        return sdk_resources_.FileExists(file_path) || platform::FileSystem::FileExists(file_path);
    }

    ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &file_path) override {
        auto data = sdk_resources_.OpenFile(file_path);
        if (data.get()) return data;
        return platform::FileSystem::OpenFile(file_path);
    }

  private:
    resources::embedded::SDKResources sdk_resources_;
};

namespace platform_detail {
inline void setup(ultralight::Config *cfg, bool chinese) {
    // Platform stores borrowed service pointers. Keep the services alive for the process.
    // Both public entrypoints share this guard; the first initialization selects the font/config.
    static const ultralight::Config config = cfg ? *cfg : ultralight::Config {};
    static const auto &font =
        chinese ? bin2cpp::getAlimamaShuHeiTiBoldTtfFile() : bin2cpp::getFreeUniversalRegularTtfFile();
    static platform::MemoryFontLoader loader(
        {{font.getFileName(),
          ultralight::FontFile::Create(ultralight::Buffer::Create(
              const_cast<char *>(font.getBuffer()), font.getSize(), nullptr, nullptr))}});
    static EmbeddedResourceFileSystem file_system("./assets/", config.resource_path_prefix.utf8().data());
    static const bool initialized = [&] {
        auto &platform = ultralight::Platform::instance();
        platform.set_config(config);
        platform.set_font_loader(&loader);
        platform.set_file_system(&file_system);
        platform.set_logger(StdoutLogger::instance());
        return true;
    }();
    (void)initialized;
}
} // namespace platform_detail

inline void setup_ultralight_platform(ultralight::Config *cfg = nullptr) {
    platform_detail::setup(cfg, false);
}

inline void setup_ultralight_platform_with_chinese_font(ultralight::Config *cfg = nullptr) {
    platform_detail::setup(cfg, true);
}
} // namespace ulbind17
