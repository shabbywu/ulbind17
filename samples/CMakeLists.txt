function(ulbind17_add_sample target directory)
  add_executable(${target} WIN32 MACOSX_BUNDLE "${directory}/main.cpp")
  target_link_libraries(${target} PRIVATE ulbind17::ulbind17)
  if(APPLE)
    set_target_properties(
      ${target}
      PROPERTIES MACOSX_BUNDLE_INFO_PLIST
                 "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/Info.plist.in")
  endif()
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8)
    set_target_properties(
      ${target} PROPERTIES LINK_FLAGS
                           "/ENTRY:mainCRTStartup /subsystem:console")
  endif()
  ulbind17_copy_runtime_files(${target})
  if(BUILD_TESTING AND NOT target STREQUAL "Sample5")
    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(
      ${target} PROPERTIES TIMEOUT 20 WORKING_DIRECTORY
                           "${CMAKE_CURRENT_BINARY_DIR}/isolated")
  endif()
endfunction()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/isolated")
ulbind17_add_sample(Sample1 "Sample 1 - CppFunction")
ulbind17_add_sample(Sample2 "Sample 2 - JSArray")
ulbind17_add_sample(Sample3 "Sample 3 - JSObject")
ulbind17_add_sample(Sample4 "Sample 4 - CppClass")
add_executable(HostAdapter host_adapter.cpp)
target_link_libraries(HostAdapter PRIVATE ulbind17::ulbind17)
ulbind17_copy_runtime_files(HostAdapter INCLUDE_APPCORE OFF)
if(BUILD_TESTING)
  add_test(NAME HostAdapter COMMAND HostAdapter)
  set_tests_properties(
    HostAdapter PROPERTIES TIMEOUT 20 WORKING_DIRECTORY
                           "${CMAKE_CURRENT_BINARY_DIR}/isolated")
endif()
if(ULBIND17_BUILD_RENDERING_SAMPLE)
  find_package(glfw3 CONFIG REQUIRED)
  find_package(glad CONFIG REQUIRED)
  ulbind17_add_sample(Sample5 "Sample 5 - Rendering")
  target_sources(Sample5 PRIVATE "Sample 5 - Rendering/gui.cpp")
  target_link_libraries(Sample5 PRIVATE glfw glad::glad)
  if(BUILD_TESTING)
    set(_smoke_arguments --smoke-test)
    if(ULBIND17_ALLOW_UNAVAILABLE_GRAPHICS)
      list(APPEND _smoke_arguments --allow-unavailable-graphics)
    endif()
    add_test(NAME Sample5.smoke COMMAND $<TARGET_FILE:Sample5>
                                        ${_smoke_arguments})
    set_tests_properties(
      Sample5.smoke PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 WORKING_DIRECTORY
                               "${CMAKE_CURRENT_BINARY_DIR}/isolated")
  endif()
endif()

if(ULBIND17_BUILD_APPCORE_SAMPLE)
  add_executable(AppCoreExample MACOSX_BUNDLE appcore.cpp)
  target_link_libraries(AppCoreExample PRIVATE ulbind17::appcore)
  if(MSVC)
    target_compile_options(AppCoreExample PRIVATE /utf-8)
  endif()
  ulbind17_copy_runtime_files(AppCoreExample INCLUDE_APPCORE ON)
  if(APPLE)
    set(_appcore_assets
        "$<TARGET_BUNDLE_CONTENT_DIR:AppCoreExample>/Resources/assets/resources"
    )
    set_target_properties(
      AppCoreExample PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER
                                "com.ulbind17.AppCoreExample")
  else()
    set(_appcore_assets "$<TARGET_FILE_DIR:AppCoreExample>/assets/resources")
  endif()
  add_custom_command(
    TARGET AppCoreExample
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_appcore_assets}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_RESOURCES_DIR}"
            "${_appcore_assets}"
    VERBATIM)
  if(BUILD_TESTING)
    add_test(NAME AppCoreExample.smoke COMMAND $<TARGET_FILE:AppCoreExample>
                                               --smoke-test)
    set_tests_properties(
      AppCoreExample.smoke
      PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE WORKING_DIRECTORY
                 "${CMAKE_CURRENT_BINARY_DIR}/isolated")
  endif()
endif()

if(ULBIND17_BUILD_OFFICIAL_SAMPLES OR ULBIND17_BUILD_GAME_UI_HOST_SAMPLE)
  add_subdirectory(official)
endif()
