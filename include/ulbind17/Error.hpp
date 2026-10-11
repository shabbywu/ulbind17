#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace ulbind17 {
enum class ErrorCode { InvalidArgument, WrongThread, Closed, Busy, PageGone, CrossContext,
                       ScriptException, HostException, Unsupported, TypeMismatch, TraversalLimit };

// Owned diagnostics: reading an Error never requires a live JavaScript document.
class Error : public std::runtime_error {
  public:
    ErrorCode code;
    std::string stack, source_url;
    double line = 0, column = 0;
    Error(ErrorCode code, std::string message) : std::runtime_error(std::move(message)), code(code) {}
};
} // namespace ulbind17
