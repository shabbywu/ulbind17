cmake_minimum_required(VERSION 3.22)
project(ultralight-sdk)
include(FetchContent)

message(STATUS "downloading ultralight-sdk...")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(
    DOWNLOAD
    https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-linux-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 97e76515344bb99c4279796bec6d2120)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  file(
    DOWNLOAD
    https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-win-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 74fb189d899fe864382bc68775a1c760)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    file(
      DOWNLOAD
      https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-mac-x64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 69ff45ed6f190e734163a6f9815681c7)
  else()
    file(
      DOWNLOAD
      https://ultralight-sdk-dev.sfo2.cdn.digitaloceanspaces.com/ultralight-sdk-min-latest-mac-arm64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 987f756206d85ff003e1134324a60354)
  endif()
else()
  message(FATAL_ERROR "Not implemented support for ${CMAKE_SYSTEM_NAME}")
endif()

file(ARCHIVE_EXTRACT INPUT ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk)

add_library(ultralight-sdk INTERFACE)
target_link_directories(
  ultralight-sdk INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/bin"
  "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/lib")
target_include_directories(
  ultralight-sdk INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/include")
target_link_libraries(ultralight-sdk INTERFACE UltralightCore Ultralight
                                               WebCore)
set_target_properties(
  ultralight-sdk
  PROPERTIES ULTRALIGHT_BINARY_DIR
             "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/bin"
             ULTRALIGHT_INSPECTOR_DIR
             "${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk/inspector")

add_library(ultralight-sdk-fullstack INTERFACE)
target_link_libraries(ultralight-sdk-fullstack INTERFACE ultralight-sdk AppCore)
