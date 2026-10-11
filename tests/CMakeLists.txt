find_package(Threads REQUIRED)

add_executable(ulbind17_minmax_tests minmax.cpp minmax_undefined.cpp)
target_include_directories(ulbind17_minmax_tests PRIVATE
  "${ULBINDING_INCLUDE_DIR}" "${EXTRA_INCLUDE_DIR}")
target_compile_features(ulbind17_minmax_tests PRIVATE cxx_std_20)
add_test(NAME platform.minmax COMMAND ulbind17_minmax_tests)

add_executable(ulbind17_bridge_tests bridge.cpp)
target_link_libraries(ulbind17_bridge_tests PRIVATE ulbind17::ulbind17)
target_compile_definitions(ulbind17_bridge_tests PRIVATE
  "ULBIND17_SDK_RESOURCES=\"${ULTRALIGHT_RESOURCES_DIR}\"")
if(MSVC)
  target_compile_options(ulbind17_bridge_tests PRIVATE /utf-8)
endif()
ulbind17_copy_runtime_files(ulbind17_bridge_tests)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")
foreach(_case IN ITEMS values lifetime navigation resources chinese)
  add_test(NAME bridge.${_case} COMMAND ulbind17_bridge_tests ${_case})
  set_tests_properties(bridge.${_case} PROPERTIES TIMEOUT 20
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")
endforeach()

add_executable(ulbind17_pixels_tests pixels.cpp)
target_link_libraries(ulbind17_pixels_tests PRIVATE ulbind17::ulbind17)
ulbind17_copy_runtime_files(ulbind17_pixels_tests INCLUDE_APPCORE OFF)
add_test(NAME runtime.pixels COMMAND ulbind17_pixels_tests)
set_tests_properties(runtime.pixels PROPERTIES TIMEOUT 20
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")

foreach(_backend IN ITEMS filesystem embedded)
  add_executable(ulbind17_overlay_${_backend} overlay.cpp)
  target_link_libraries(ulbind17_overlay_${_backend} PRIVATE ulbind17::header Threads::Threads)
  target_compile_definitions(ulbind17_overlay_${_backend} PRIVATE
    "ULBIND17_SDK_RESOURCES=\"${ULTRALIGHT_RESOURCES_DIR}\"")
  if(_backend STREQUAL "embedded")
    target_link_libraries(ulbind17_overlay_${_backend} PRIVATE ulbind17::ulbind17)
    target_compile_definitions(ulbind17_overlay_${_backend} PRIVATE ULBIND17_TEST_EMBEDDED)
  endif()
  ulbind17_copy_runtime_files(ulbind17_overlay_${_backend} INCLUDE_APPCORE OFF)
  add_test(NAME resources.overlay.${_backend} COMMAND ulbind17_overlay_${_backend})
endforeach()

add_executable(ulbind17_runtime_loader runtime_loader.cpp)
target_link_libraries(ulbind17_runtime_loader PRIVATE ${CMAKE_DL_LIBS})
target_compile_features(ulbind17_runtime_loader PRIVATE cxx_std_20)
foreach(_kind IN ITEMS SHARED MODULE)
  string(TOLOWER "${_kind}" _name)
  add_library(ulbind17_probe_${_name} ${_kind} runtime_probe.cpp)
  target_link_libraries(ulbind17_probe_${_name} PRIVATE ulbind17::header)
  set_target_properties(ulbind17_probe_${_name} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/probe-${_name}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/probe-${_name}")
  ulbind17_copy_runtime_files(ulbind17_probe_${_name} INCLUDE_APPCORE OFF)
  add_test(NAME runtime.load.${_name} COMMAND ulbind17_runtime_loader "$<TARGET_FILE:ulbind17_probe_${_name}>")
endforeach()
foreach(_case IN ITEMS cache checksum cached-checksum old-version platform download-error)
  add_test(NAME sdk.${_case} COMMAND ${CMAKE_COMMAND}
    "-DCASE=${_case}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/sdk-${_case}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/sdk.cmake")
endforeach()

# The filesystem backend must link without the embedded SDK resource arrays.
add_executable(ulbind17_javascript_tests javascript.cpp)
target_link_libraries(ulbind17_javascript_tests PRIVATE ulbind17::ulbind17 Threads::Threads)
if(MSVC)
  target_compile_options(ulbind17_javascript_tests PRIVATE /utf-8)
endif()
ulbind17_copy_runtime_files(ulbind17_javascript_tests)
foreach(_case IN ITEMS conveniences scaffold async expiry)
  add_test(NAME javascript.${_case} COMMAND ulbind17_javascript_tests ${_case})
  set_tests_properties(javascript.${_case} PROPERTIES TIMEOUT 20
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")
endforeach()

add_executable(ulbind17_filesystem_resources_tests sdk_resources_filesystem.cpp)
target_link_libraries(ulbind17_filesystem_resources_tests PRIVATE ulbind17::header)
target_compile_definitions(ulbind17_filesystem_resources_tests PRIVATE
  "ULBIND17_SDK_RESOURCES=\"${ULTRALIGHT_RESOURCES_DIR}\"")
ulbind17_copy_runtime_files(ulbind17_filesystem_resources_tests)
add_test(NAME resources.filesystem COMMAND ulbind17_filesystem_resources_tests)
set_tests_properties(resources.filesystem PROPERTIES TIMEOUT 20
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")

add_executable(ulbind17_runtime_tests runtime.cpp)
target_link_libraries(ulbind17_runtime_tests PRIVATE ulbind17::ulbind17 Threads::Threads)
ulbind17_copy_runtime_files(ulbind17_runtime_tests INCLUDE_APPCORE OFF)
foreach(_case IN ITEMS dynamic lifecycle threading)
  add_test(NAME runtime.${_case} COMMAND ulbind17_runtime_tests ${_case})
  set_tests_properties(runtime.${_case} PROPERTIES TIMEOUT 20
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")
endforeach()
