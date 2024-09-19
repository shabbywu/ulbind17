#pragma once
#include <cstring>

#include "fonts/FreeUniversal-Regular.h"
#include "resources/cacert.h"
#include "resources/icudt67l.h"
#include <Ultralight/Ultralight.h>
#include <iostream>
#include <ulbind17/platform/FileSystem.hpp>
#include <ulbind17/platform/FontLoader.hpp>

using namespace ultralight;

namespace ulbind17 {
class StdoutLogger : public ultralight::Logger {

  public:
    virtual void LogMessage(LogLevel log_level, const String &message) {
        std::cout << message.utf8().data() << std::endl;
    }

    static StdoutLogger *instance() {
        static StdoutLogger instance;
        return &instance;
    }
};

class EmbeddedResourceFileSystem : public platform::FileSystem {
    using platform::FileSystem::FileSystem;
    virtual bool FileExists(const ultralight::String &file_path) override {
        std::string p = file_path.utf8().data();
        {
            auto &icudt67l = bin2cpp::getIcudt67lDatFile();
            if (p == icudt67l.getFileName()) {
                return true;
            }
        }
        {
            auto &cacert = bin2cpp::getCacertPemFile();
            if (p == cacert.getFileName()) {
                return true;
            }
        }
        return platform::FileSystem::FileExists(file_path);
    }

    virtual ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String &file_path) override {
        std::string p = file_path.utf8().data();
        {
            auto &icudt67l = bin2cpp::getIcudt67lDatFile();
            if (p == icudt67l.getFileName()) {
                return ultralight::Buffer::Create((void *)icudt67l.getBuffer(), icudt67l.getSize(), nullptr, nullptr);
            }
        }

        {
            auto &cacert = bin2cpp::getIcudt67lDatFile();
            if (p == cacert.getFileName()) {
                return ultralight::Buffer::Create((void *)cacert.getBuffer(), cacert.getSize(), nullptr, nullptr);
            }
        }
        return platform::FileSystem::OpenFile(file_path);
    }
};

static void setup_ultralight_platform() {
    static bool inited = false;
    if (inited)
        return;
    // Get the Platform singleton (maintains global library state)
    auto &platform = Platform::instance();

    // Setup platform
    Config my_config;
    platform.set_config(my_config);
    auto &font = bin2cpp::getFreeUniversalRegularTtfFile();
    platform.set_font_loader(new platform::MemoryFontLoader(
        {{font.getFileName(),
          FontFile::Create(Buffer::Create((void *)font.getBuffer(), font.getSize(), nullptr, nullptr))}}));
    platform.set_file_system(new EmbeddedResourceFileSystem("./assets/"));

    platform.set_logger(StdoutLogger::instance());
    inited = true;
}

} // namespace ulbind17
