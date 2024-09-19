project(Samples C CXX)

cmake_minimum_required(VERSION 3.3.2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ULTRALIGHT_INCLUDE_DIR "${SDK_ROOT}/include")
set(ULTRALIGHT_BINARY_DIR "${SDK_ROOT}/bin")
set(ULTRALIGHT_INSPECTOR_DIR "${SDK_ROOT}/inspector")
set(ULTRALIGHT_RESOURCES_DIR "${SDK_ROOT}/resources")

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(ULTRALIGHT_LIBRARY_DIR "${SDK_ROOT}/lib")
else()
  set(ULTRALIGHT_LIBRARY_DIR "${SDK_ROOT}/bin")
endif()

target_link_directories(ulbind17 INTERFACE ${ULTRALIGHT_LIBRARY_DIR})
target_include_directories(ulbind17 INTERFACE ${ULTRALIGHT_INCLUDE_DIR})

if(PORT MATCHES "UltralightMac")
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH "@executable_path/")
endif()

if(PORT MATCHES "UltralightLinux")
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH "$\{ORIGIN\}")
endif()

add_subdirectory("Sample 1 - CppFunction")
add_subdirectory("Sample 2 - JSArray")
add_subdirectory("Sample 3 - JSObject")
add_subdirectory("Sample 4 - CppClass")
add_subdirectory("Sample 5 - Rendering")
