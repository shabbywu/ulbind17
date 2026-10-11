set(ULTRALIGHT_SDK_ROOT "" CACHE PATH "Path to an extracted Ultralight 2.x SDK")
set(ULTRALIGHT_SDK_VERSION "latest" CACHE STRING "Ultralight SDK package version to download")
set(ULTRALIGHT_SDK_API_URL "https://ultralig.ht/api/v1/sdk" CACHE STRING "Ultralight SDK API endpoint")

include(${CMAKE_CURRENT_LIST_DIR}/SDK.cmake)
ulbind17_sdk_platform(_sdk_platform)
if(NOT TARGET Ultralight::Ultralight)
  set(_promote_imports TRUE)
endif()

if(ULTRALIGHT_SDK_ROOT)
  set(Ultralight_DIR "${ULTRALIGHT_SDK_ROOT}/lib/cmake/Ultralight" CACHE PATH "Ultralight package" FORCE)
  find_package(Ultralight 2 CONFIG REQUIRED
    PATHS "${ULTRALIGHT_SDK_ROOT}/lib/cmake/Ultralight" NO_DEFAULT_PATH)
elseif((Ultralight_DIR AND NOT Ultralight_DIR STREQUAL ULBIND17_DOWNLOADED_SDK_DIR)
    OR TARGET Ultralight::Ultralight)
  find_package(Ultralight 2 CONFIG REQUIRED)
else()
  if(NOT ULBIND17_DOWNLOADED_SDK_DIR)
    find_package(Ultralight 2 CONFIG QUIET)
  endif()
  if(NOT Ultralight_FOUND)
    ulbind17_download_sdk(_sdk_root "${_sdk_platform}" "${ULTRALIGHT_SDK_VERSION}"
      "${CMAKE_CURRENT_BINARY_DIR}/sdk-cache")
    set(Ultralight_DIR "${_sdk_root}/lib/cmake/Ultralight" CACHE PATH "Ultralight package" FORCE)
    find_package(Ultralight 2 CONFIG REQUIRED
      PATHS "${_sdk_root}/lib/cmake/Ultralight" NO_DEFAULT_PATH)
    set(ULBIND17_DOWNLOADED_SDK_DIR "${Ultralight_DIR}" CACHE INTERNAL "Automatically downloaded SDK" FORCE)
  endif()
endif()

if(_promote_imports)
  foreach(_library IN ITEMS UltralightCore WebCore Ultralight AppCore)
    set_target_properties(Ultralight::${_library} PROPERTIES IMPORTED_GLOBAL TRUE)
  endforeach()
endif()

if(NOT ULTRALIGHT_VERSION MATCHES "^2\\.")
  message(FATAL_ERROR "ulbind17 requires an Ultralight 2.x SDK; found ${ULTRALIGHT_VERSION}")
endif()
get_filename_component(_sdk_root "${ULTRALIGHT_INCLUDE_DIR}/.." ABSOLUTE)
file(READ "${_sdk_root}/VERSION.txt" _package_version)
string(STRIP "${_package_version}" _package_version)
message(STATUS "Ultralight SDK: ${_package_version} (${_sdk_root})")

add_library(ultralight-sdk INTERFACE)
target_link_libraries(ultralight-sdk INTERFACE Ultralight::Ultralight)
set_target_properties(ultralight-sdk PROPERTIES
  ULTRALIGHT_BINARY_DIR "${ULTRALIGHT_BINARY_DIR}"
  ULTRALIGHT_INSPECTOR_DIR "${ULTRALIGHT_INSPECTOR_DIR}")
add_library(ultralight-sdk-fullstack INTERFACE)
target_link_libraries(ultralight-sdk-fullstack INTERFACE ultralight-sdk Ultralight::AppCore)

foreach(_variable IN ITEMS ULTRALIGHT_INCLUDE_DIR ULTRALIGHT_BINARY_DIR ULTRALIGHT_RESOURCES_DIR)
  set(${_variable} "${${_variable}}" PARENT_SCOPE)
endforeach()

# Embedded applications only need the libraries, not a second copy of the data files.
function(ulbind17_copy_runtime_files target)
  cmake_parse_arguments(PARSE_ARGV 1 _runtime "" "INCLUDE_APPCORE" "")
  if(_runtime_UNPARSED_ARGUMENTS OR _runtime_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR "ulbind17_copy_runtime_files: expected INCLUDE_APPCORE ON or OFF")
  endif()
  if(NOT DEFINED _runtime_INCLUDE_APPCORE)
    set(_runtime_INCLUDE_APPCORE ON)
  endif()
  get_target_property(_target_type ${target} TYPE)
  if(NOT _target_type MATCHES "^(EXECUTABLE|SHARED_LIBRARY|MODULE_LIBRARY)$")
    message(FATAL_ERROR "ulbind17_copy_runtime_files requires an executable, shared or module library")
  endif()
  set(_runtime_libraries UltralightCore WebCore Ultralight)
  if(_runtime_INCLUDE_APPCORE)
    list(APPEND _runtime_libraries AppCore)
  endif()
  foreach(_library IN LISTS _runtime_libraries)
    get_target_property(_library_type Ultralight::${_library} TYPE)
    if(_library_type STREQUAL "SHARED_LIBRARY")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:Ultralight::${_library}>" "$<TARGET_FILE_DIR:${target}>"
        VERBATIM)
    endif()
  endforeach()
  if(APPLE)
    set_target_properties(${target} PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "@loader_path/")
  elseif(UNIX)
    set_target_properties(${target} PROPERTIES
      BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN")
  endif()
endfunction()
