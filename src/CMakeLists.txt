# Add subdirectories for modular libraries
add_subdirectory(core)
add_subdirectory(capture)
# Detection, input, tracking, config, ui will be added as Phase 2+ progresses

# Main Executable (minimal Phase 2 version)
add_executable(macroman_aimbot main.cpp)

# Enable C++/WinRT for main
if(MSVC)
    target_compile_options(macroman_aimbot PRIVATE /await)
    target_link_libraries(macroman_aimbot PRIVATE windowsapp)
endif()

target_link_libraries(macroman_aimbot PRIVATE
    macroman_core
    macroman_capture
    spdlog::spdlog
    d3d11.lib
    dxgi.lib
)

# Copy shaders to bin (will be populated in detection module tasks)
# file(GLOB HLSL_SHADERS "${CMAKE_SOURCE_DIR}/src/detection/directml/*.hlsl")
# foreach(SHADER ${HLSL_SHADERS})
#     add_custom_command(TARGET macroman_aimbot POST_BUILD
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${SHADER}"
#         "$<TARGET_FILE_DIR:macroman_aimbot>/"
#         COMMENT "Copying shader ${SHADER}"
#     )
# endforeach()