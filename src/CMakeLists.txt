
# Add submodules
add_subdirectory(core)
add_subdirectory(capture)
add_subdirectory(detection)
add_subdirectory(input)
add_subdirectory(tracking)
add_subdirectory(config)
add_subdirectory(ui)

# Main Executable
add_executable(MarcomanAimbot main.cpp)

target_link_libraries(MarcomanAimbot PRIVATE
    aimbot_core
    aimbot_capture
    aimbot_detection
    aimbot_input
    aimbot_config
    aimbot_ui
    imgui
    simpleini
    glfw
    serial
    OpenCV::opencv_core
    OpenCV::opencv_imgproc
    OpenCV::opencv_highgui
    d3d11.lib
    dxgi.lib
)

# Copy shaders to bin
file(GLOB HLSL_SHADERS "${CMAKE_SOURCE_DIR}/src/detection/directml/*.hlsl")
foreach(SHADER ${HLSL_SHADERS})
    add_custom_command(TARGET MarcomanAimbot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHADER}"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$(Configuration)/"
    )
endforeach()
