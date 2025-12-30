# Add subdirectories for modular libraries
add_subdirectory(core)
add_subdirectory(capture)
add_subdirectory(detection)
add_subdirectory(input)
add_subdirectory(tracking)
add_subdirectory(config)
add_subdirectory(ui)

# Main Executable
add_executable(MarcomanAimbot main.cpp)

# Enable C++/WinRT for main
if(MSVC)
    target_compile_options(MarcomanAimbot PRIVATE /await)
    target_link_libraries(MarcomanAimbot PRIVATE windowsapp)
endif()

target_link_libraries(MarcomanAimbot PRIVATE
    aimbot_core
    aimbot_capture
    aimbot_detection
    aimbot_input
    aimbot_tracking
    aimbot_config
    aimbot_ui
    imgui
    simpleini
    glfw
    serial
    spdlog::spdlog
    ${OpenCV_LIBRARIES}
    d3d11.lib
    dxgi.lib
)

# Copy shaders to bin
file(GLOB HLSL_SHADERS "${CMAKE_SOURCE_DIR}/src/detection/directml/*.hlsl")
foreach(SHADER ${HLSL_SHADERS})
    add_custom_command(TARGET MarcomanAimbot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER}"
        "$<TARGET_FILE_DIR:MarcomanAimbot>/"
        COMMENT "Copying shader ${SHADER}"
    )
endforeach()