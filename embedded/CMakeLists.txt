set(_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/ulbind17/resources")
set(_resource_sources)
foreach(_resource IN ITEMS icudt67l cacert)
  if(_resource STREQUAL "icudt67l")
    set(_filename "icudt67l.dat")
  else()
    set(_filename "cacert.pem")
  endif()
  set(_input "${ULTRALIGHT_RESOURCES_DIR}/${_filename}")
  if(NOT EXISTS "${_input}")
    message(FATAL_ERROR "SDK resource is missing: ${_input}")
  endif()
  set(_output "${_generated}/${_resource}.cpp")
  add_custom_command(OUTPUT "${_output}"
    COMMAND ${CMAKE_COMMAND} "-DINPUT=${_input}" "-DOUTPUT=${_output}" "-DNAME=${_resource}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateResource.cmake"
    DEPENDS "${_input}" "${CMAKE_CURRENT_SOURCE_DIR}/GenerateResource.cmake"
    VERBATIM)
  list(APPEND _resource_sources "${_output}")
endforeach()

add_library(embedded_resources STATIC
  ulbind17/fonts/FreeUniversal-Regular.cpp
  ulbind17/fonts/AlimamaShuHeiTi-Bold.cpp
  ${_resource_sources})
target_include_directories(embedded_resources PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(embedded_resources PUBLIC cxx_std_20)
target_link_libraries(embedded_resources PUBLIC ultralight-sdk)
set_target_properties(embedded_resources PROPERTIES POSITION_INDEPENDENT_CODE ON)
