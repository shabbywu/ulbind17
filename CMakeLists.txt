project(Samples C CXX)

cmake_minimum_required(VERSION 3.3.2)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IN_SOURCE_BUILD 0)
set(SDK_ROOT ${CMAKE_CURRENT_LIST_DIR}/sdk_root)
set(ULBINDING_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(EXTRA_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/externals/include)
set(EXTRA_LIBRARY_DIR ${CMAKE_CURRENT_LIST_DIR}/externals/libs)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(ULTRALIGHT_LIBRARY_DIR "${SDK_ROOT}/lib")
else()
  set(ULTRALIGHT_LIBRARY_DIR "${SDK_ROOT}/bin")
endif()

include_directories("${ULBINDING_INCLUDE_DIR}" "${SDK_ROOT}/include"
                    ${CMAKE_CURRENT_LIST_DIR}/embedded)

option(ENABLE_TEST "enable test?" FALSE)

add_library(ulbind17_header INTERFACE)
target_link_directories(ulbind17_header INTERFACE ${EXTRA_LIBRARY_DIR}
                        ${ULTRALIGHT_LIBRARY_DIR})
target_include_directories(
  ulbind17_header INTERFACE ${ULBINDING_INCLUDE_DIR} ${EXTRA_INCLUDE_DIR}
                            ${SDK_ROOT}/include)
target_link_libraries(ulbind17_header INTERFACE UltralightCore Ultralight
                                                WebCore)
add_library(ulbind17::header ALIAS ulbind17_header)

add_subdirectory(embedded)
add_library(ulbind17 INTERFACE)
target_link_libraries(ulbind17 INTERFACE ulbind17_header embedded_resources)
add_library(ulbind17::ulbind17 ALIAS ulbind17)

if(${ENABLE_TEST})
  add_subdirectory(samples)
endif()
