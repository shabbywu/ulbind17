cmake_minimum_required(VERSION 3.22)
project(ultralight-sdk)
include(FetchContent)

add_library(ultralight-sdk INTERFACE)

message(STATUS "downloading ultralight-sdk...")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(
    DOWNLOAD
    https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-linux-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 a45bf06769302737a266129acf5e8c2f)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  file(
    DOWNLOAD
    https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-win-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 24be16afe81f202743d05dbdeead1b54)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    file(
      DOWNLOAD
      https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-mac-x64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 eafbb8b5e055123fd3a17197521a6d2f)
  else()
    file(
      DOWNLOAD
      https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-mac-arm64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 5d9f67d35f8acead7e3912986314eaf6)
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
