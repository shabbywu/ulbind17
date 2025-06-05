cmake_minimum_required(VERSION 3.22)
project(ultralight-sdk)
include(FetchContent)

message(STATUS "downloading ultralight-sdk...")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(
    DOWNLOAD
    https://github.com/shabbywu/ulbind17/releases/download/ultralight-free-sdk-1.4.0/ultralight-free-sdk-1.4.0-linux-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 2647ea26984881ae56e5c4b5960a0904)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  file(
    DOWNLOAD
    https://github.com/shabbywu/ulbind17/releases/download/ultralight-free-sdk-1.4.0/ultralight-free-sdk-1.4.0-win-x64.7z
    ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
    SHOW_PROGRESS
    EXPECTED_MD5 a209d25754e352a35b33c76e52b83bf6)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    file(
      DOWNLOAD
      https://github.com/shabbywu/ulbind17/releases/download/ultralight-free-sdk-1.4.0/ultralight-free-sdk-1.4.0-mac-x64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 ed3b9ecbd1396d472f0674524385a650)
  else()
    file(
      DOWNLOAD
      https://github.com/shabbywu/ulbind17/releases/download/ultralight-free-sdk-1.4.0/ultralight-free-sdk-1.4.0-mac-arm64.7z
      ${CMAKE_CURRENT_BINARY_DIR}/ultralight-sdk.7z
      SHOW_PROGRESS
      EXPECTED_MD5 5dbb990cac22aeb74e6a62c9a64b62b2)
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
