cmake_minimum_required(VERSION 3.22)
project(ultralight-sdk)
include(FetchContent)

add_library(ultralight-sdk INTERFACE)

message(STATUS "downloading ultralight-sdk...")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(
    DOWNLOAD
    https://github.com/ultralight-ux/Ultralight/releases/download/v1.4.0-beta/ultralight-sdk-1.4.0b-linux-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 74302987a6e0ae3ce1dddf4bedbe600f)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  file(
    DOWNLOAD
    https://github.com/ultralight-ux/Ultralight/releases/download/v1.4.0-beta/ultralight-sdk-1.4.0b-win-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 6b2de2e4b341503909f3635edf50e926)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    message(STATUS "Building for x86_64 or Universal")
    file(
      DOWNLOAD
      https://github.com/ultralight-ux/Ultralight/releases/download/v1.4.0-beta/ultralight-sdk-1.4.0b-mac-x64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 318b3cccf30b82dee21085d08b2e2200)
  else()
    message(STATUS "Building for ARM")
    file(
      DOWNLOAD
      https://github.com/ultralight-ux/Ultralight/releases/download/v1.4.0-beta/ultralight-sdk-1.4.0b-mac-arm64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 eb151b5abb34f0ebd0be97139fc2e725)
  endif()
else()
  message(FATAL_ERROR "Not implemented support for ${CMAKE_SYSTEM_NAME}")
endif()

file(ARCHIVE_EXTRACT INPUT ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk)

target_link_directories(
  ultralight-sdk INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/bin"
  "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/lib")
target_include_directories(
  ultralight-sdk INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/include")
target_link_libraries(ultralight-sdk INTERFACE UltralightCore Ultralight
                                               WebCore)
set_target_properties(
  ultralight-sdk PROPERTIES ULTRALIGHT_BINARY_DIR
                            "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/bin")
