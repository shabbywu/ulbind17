cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

get_target_property(ULTRALIGHT_BINARY_DIR ultralight-sdk ULTRALIGHT_BINARY_DIR)

add_subdirectory("Sample 1 - CppFunction")
add_subdirectory("Sample 2 - JSArray")
add_subdirectory("Sample 3 - JSObject")
add_subdirectory("Sample 4 - CppClass")
add_subdirectory("Sample 5 - Rendering")
