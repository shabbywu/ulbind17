set(APP_NAME Sample5)

add_library(GLAD glad.c)
target_include_directories(GLAD PUBLIC ${EXTRA_INCLUDE_DIR})

add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE "main.cpp" "gui.cpp")
target_link_libraries(${APP_NAME} ulbind17::ulbind17 glfw3 GLAD)

if(APPLE)
  # Enable High-DPI on macOS through our custom Info.plist template
  set_target_properties(
    ${APP_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST
                           ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in)
endif()

if(MSVC)
  # Tell MSVC to use main instead of WinMain for Windows subsystem executables
  set_target_properties(
    ${APP_NAME} PROPERTIES LINK_FLAGS
                           "/ENTRY:mainCRTStartup /subsystem:console")
endif()

# Copy all binaries to target directory
add_custom_command(
  TARGET ${APP_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${ULTRALIGHT_BINARY_DIR}"
          $<TARGET_FILE_DIR:${APP_NAME}>)

# Set the assets path to "/assets" or "/../Resources/assets" on macOS
if(APPLE)
  set(ASSETS_PATH "$<TARGET_FILE_DIR:${APP_NAME}>/../Resources/assets")
else()
  set(ASSETS_PATH "$<TARGET_FILE_DIR:${APP_NAME}>/assets")
endif()
