cmake_minimum_required(VERSION 3.22)
project(ultralight-sdk)
include(FetchContent)

add_library(ultralight-sdk INTERFACE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  FetchContent_Declare(
    ultralight-sdk-package
    URL https://github.com/ultralight-ux/Ultralight/releases/download/v1.4.0-beta/ultralight-sdk-1.4.0b-linux-x64.7z
        # URL_HASH MD5=
        SOURCE_SUBDIR
        _
        EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(ultralight-sdk-package)
  target_link_directories(ultralight-sdk INTERFACE
                          "${ultralight-sdk-package_SOURCE_DIR}/bin")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  FetchContent_Declare(
    ultralight-sdk-package
    URL https://github.com/ultralight-ux/Ultralight/releases/download/v1.4.0-beta/ultralight-sdk-1.4.0b-win-x64.7z
        # URL_HASH MD5=
        SOURCE_SUBDIR
        _
        EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(ultralight-sdk-package)
  target_link_directories(ultralight-sdk INTERFACE
                          "${ultralight-sdk-package_SOURCE_DIR}/lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  FetchContent_Declare(
    ultralight-sdk-package
    URL https://github.com/ultralight-ux/Ultralight/releases/download/v1.4.0-beta/ultralight-sdk-1.4.0b-mac-x64.7z
        # URL_HASH MD5=
        SOURCE_SUBDIR
        _
        EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(ultralight-sdk-package)
  target_link_directories(ultralight-sdk INTERFACE
                          "${ultralight-sdk-package_SOURCE_DIR}/bin")
else()
  message(FATAL_ERROR "Not implemented support for ${CMAKE_SYSTEM_NAME}")
endif()

target_include_directories(
  ultralight-sdk INTERFACE "${ultralight-sdk-package_SOURCE_DIR}/include")
target_link_libraries(ultralight-sdk INTERFACE UltralightCore Ultralight
                                               WebCore)
set_target_properties(
  ultralight-sdk PROPERTIES ULTRALIGHT_BINARY_DIR
                            "${ultralight-sdk-package_SOURCE_DIR}/bin")
