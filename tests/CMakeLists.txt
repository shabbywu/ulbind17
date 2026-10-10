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
foreach(_case IN ITEMS cache checksum cached-checksum old-version platform download-error)
  add_test(NAME sdk.${_case} COMMAND ${CMAKE_COMMAND}
    "-DCASE=${_case}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/sdk-${_case}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/sdk.cmake")
endforeach()

# The filesystem backend must link without the embedded SDK resource arrays.
add_executable(ulbind17_filesystem_resources_tests sdk_resources_filesystem.cpp)
target_link_libraries(ulbind17_filesystem_resources_tests PRIVATE ulbind17::header)
target_compile_definitions(ulbind17_filesystem_resources_tests PRIVATE
  "ULBIND17_SDK_RESOURCES=\"${ULTRALIGHT_RESOURCES_DIR}\"")
ulbind17_copy_runtime_files(ulbind17_filesystem_resources_tests)
add_test(NAME resources.filesystem COMMAND ulbind17_filesystem_resources_tests)
set_tests_properties(resources.filesystem PROPERTIES TIMEOUT 20
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")
