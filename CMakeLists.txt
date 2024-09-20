cmake_minimum_required(VERSION 3.22)

option(ENABLE_TEST "enable test?" FALSE)
project(ulbind17)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ULBINDING_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(EXTRA_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/externals/include)
set(EXTRA_LIBRARY_DIR ${CMAKE_CURRENT_LIST_DIR}/externals/libs)

add_subdirectory(embedded)
add_subdirectory(ultralight-sdk)

add_library(ulbind17_header INTERFACE)
target_include_directories(
  ulbind17_header INTERFACE ${ULBINDING_INCLUDE_DIR} ${EXTRA_INCLUDE_DIR}
                            ${CMAKE_CURRENT_LIST_DIR}/embedded)
target_link_libraries(ulbind17_header INTERFACE ultralight-sdk)
add_library(ulbind17::header ALIAS ulbind17_header)

add_library(ulbind17 INTERFACE)
target_link_libraries(ulbind17 INTERFACE ulbind17_header embedded_resources)
add_library(ulbind17::ulbind17 ALIAS ulbind17)

if(${ENABLE_TEST})
  link_directories(${EXTRA_LIBRARY_DIR})
  add_subdirectory(samples)
endif()
