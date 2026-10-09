cmake_minimum_required(VERSION 3.22)

option(ENABLE_TEST "Build all samples (including the GLFW rendering sample)" OFF)
project(ulbind17 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ULBINDING_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(EXTRA_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/externals/include)

add_subdirectory(ultralight-sdk)
add_subdirectory(embedded)

add_library(ulbind17_header INTERFACE)
target_include_directories(
  ulbind17_header INTERFACE ${ULBINDING_INCLUDE_DIR} ${EXTRA_INCLUDE_DIR}
                            ${CMAKE_CURRENT_LIST_DIR}/embedded)
target_link_libraries(ulbind17_header INTERFACE ultralight-sdk)
target_compile_features(ulbind17_header INTERFACE cxx_std_20)
add_library(ulbind17::header ALIAS ulbind17_header)

add_library(ulbind17 INTERFACE)
target_link_libraries(ulbind17 INTERFACE ulbind17_header embedded_resources)
add_library(ulbind17::ulbind17 ALIAS ulbind17)

if(ENABLE_TEST)
  add_subdirectory(samples)
endif()
