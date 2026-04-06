#pragma once
#include <Ultralight/platform/FontLoader.h>
#include <map>

namespace ulbind17 {
namespace platform {
/**
 * FontLoader implementation.
 */
class MemoryFontLoader : public ultralight::FontLoader {
  public:
    MemoryFontLoader() {

    };

    MemoryFontLoader(std::map<std::string, ultralight::RefPtr<ultralight::FontFile>> fonts)
        : _fonts(fonts) {

          };
    virtual ~MemoryFontLoader() {};
    virtual ultralight::String fallback_font() const override {
        auto font = _fonts.begin()->first;
        return ultralight::String(font.data());
    }
    virtual ultralight::String fallback_font_for_characters(const ultralight::String &characters, int weight,
                                                            bool italic) const override {
        return fallback_font();
    }
    virtual ultralight::RefPtr<ultralight::FontFile> Load(const ultralight::String &family, int weight,
                                                          bool italic) override {
        std::string font = family.utf8().data();
        if (auto it = _fonts.find(font); it != _fonts.end()) {
            return it->second;
        }
        return nullptr;
    }

  protected:
    std::map<std::string, ultralight::RefPtr<ultralight::FontFile>> _fonts;
};
} // namespace platform
} // namespace ulbind17
