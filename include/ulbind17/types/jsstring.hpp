#pragma once
#include <JavaScriptCore/JSRetainPtr.h>
#include <memory>
#include <string>

namespace ulbind17 {
namespace detail {

class String {
  public:
    // trim from start (in place)
    static inline void ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    }

    // trim from end (in place)
    static inline void rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    }

  private:
    struct Holder {
        JSContextRef ctx;
        JSStringRef value;
        Holder(JSContextRef ctx, JSStringRef pString) : ctx(ctx), value(pString) {
            JSRetain(value);
        }

        ~Holder() {
            JSRelease(value);
        }
    };

  protected:
    std::shared_ptr<Holder> holder;

  private:
    static JSStringRef make_string(JSContextRef ctx, std::string string) {
        return JSStringCreateWithUTF8CString(string.c_str());
    }

  public:
    String(JSContextRef ctx, JSStringRef string) : holder(std::make_shared<Holder>(ctx, string)) {};
    String(JSContextRef ctx, JSValueRef string) : String(ctx, JSValueToStringCopy(ctx, string, nullptr)) {};
    String(JSContextRef ctx, std::string string) : String(ctx, make_string(ctx, string)) {};

  public:
    std::string value() const {
        // Get the maximum conversion length
        size_t buffer_size = JSStringGetMaximumUTF8CStringSize(rawref());

        if (buffer_size < 1) {
            // Check for 0 length
            return "";
        }

        // Allocate a buffer to store the data in
        char *buffer = new char[buffer_size];

        // Convert the string
        size_t real_size = JSStringGetUTF8CString(rawref(), buffer, buffer_size);

        // Copy the buffer length into a std::string
        // This constructor takes the length without the null byte
        std::string ret(buffer, real_size - 1);

        // Clean up and return
        delete[] buffer;
        return ret;
    }
    JSStringRef rawref() const {
        return holder->value;
    }

  public:
    String &operator=(const String &other) {
        this->holder = other.holder;
        return *this;
    }

    String &operator=(const std::string &string) {
        *this = String(holder->ctx, string);
        return *this;
    }
};
} // namespace detail

} // namespace ulbind17
