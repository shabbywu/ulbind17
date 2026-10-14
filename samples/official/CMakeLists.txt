function(ulbind17_add_official_sample target directory)
  add_executable(${target} MACOSX_BUNDLE "${directory}/main.cpp")
  target_link_libraries(${target} PRIVATE ulbind17::appcore)
  set_target_properties(
    ${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                         "${CMAKE_CURRENT_BINARY_DIR}/${directory}")
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8)
  endif()
  if(APPLE)
    set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER
                                               "com.ulbind17.${target}")
    set(_assets "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/assets")
  else()
    set(_assets "$<TARGET_FILE_DIR:${target}>/assets")
  endif()
  ulbind17_copy_runtime_files(${target} INCLUDE_APPCORE ON)
  # These resources are copied into every standalone app; no build-time SDK
  # paths are embedded.
  add_custom_target(
    ${target}_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/${directory}/assets" "${_assets}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_RESOURCES_DIR}"
            "${_assets}/resources"
    VERBATIM)
  add_dependencies(${target} ${target}_assets)
  if(BUILD_TESTING)
    add_test(NAME ${target}.smoke COMMAND $<TARGET_FILE:${target}> --smoke-test)
    set_tests_properties(
      ${target}.smoke PROPERTIES TIMEOUT 45 RUN_SERIAL TRUE WORKING_DIRECTORY
                                 "${CMAKE_CURRENT_BINARY_DIR}")
  endif()
endfunction()

if(ULBIND17_BUILD_OFFICIAL_SAMPLES)
  # Learning order: desktop basics, bridge, resource loading, game UI,
  # multi-window.
  ulbind17_add_official_sample(Official02BasicApp 02-basic-app)
  ulbind17_add_official_sample(Official05JavaScriptBridge 05-javascript-bridge)
  ulbind17_add_official_sample(Official07FileLoading 07-file-loading)
  ulbind17_add_official_sample(Official13GameUI 13-game-ui)
  ulbind17_add_official_sample(Official11MultiWindow 11-multi-window)
  if(BUILD_TESTING)
    foreach(_scene IN ITEMS flight-hud overdrive lightfingers ashen-reliquary
                            wind-and-ink tidepool)
      add_test(NAME Official13GameUI.${_scene}
               COMMAND $<TARGET_FILE:Official13GameUI> --smoke-test --scene
                       ${_scene})
      set_tests_properties(
        Official13GameUI.${_scene}
        PROPERTIES TIMEOUT 45 RUN_SERIAL TRUE WORKING_DIRECTORY
                   "${CMAKE_CURRENT_BINARY_DIR}")
    endforeach()
  endif()
endif()

if(ULBIND17_BUILD_GAME_UI_HOST_SAMPLE)
  add_executable(GameUIHostAdapter 13-game-ui/host_adapter.cpp)
  target_link_libraries(GameUIHostAdapter PRIVATE ulbind17::ulbind17)
  set_target_properties(
    GameUIHostAdapter PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                 "${CMAKE_CURRENT_BINARY_DIR}/13-game-ui-host")
  if(MSVC)
    target_compile_options(GameUIHostAdapter PRIVATE /utf-8)
  endif()
  ulbind17_copy_runtime_files(GameUIHostAdapter INCLUDE_APPCORE OFF)
  add_custom_target(
    GameUIHostAdapter_assets
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/13-game-ui/assets"
      "$<TARGET_FILE_DIR:GameUIHostAdapter>/assets"
    VERBATIM)
  add_dependencies(GameUIHostAdapter GameUIHostAdapter_assets)
  if(BUILD_TESTING)
    foreach(
      _scene IN
      ITEMS chooser
            flight-hud
            overdrive
            lightfingers
            ashen-reliquary
            wind-and-ink
            tidepool)
      set(_args --smoke-test)
      if(NOT _scene STREQUAL chooser)
        list(APPEND _args --scene ${_scene})
      endif()
      add_test(NAME GameUIHostAdapter.${_scene}
               COMMAND $<TARGET_FILE:GameUIHostAdapter> ${_args})
      set_tests_properties(
        GameUIHostAdapter.${_scene}
        PROPERTIES TIMEOUT 45 WORKING_DIRECTORY
                   "$<TARGET_FILE_DIR:GameUIHostAdapter>")
    endforeach()
  endif()
endif()
