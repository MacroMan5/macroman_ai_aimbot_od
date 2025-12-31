# External Config UI Application (Standalone ImGui App)
# Creates macroman_config.exe

add_executable(macroman_config WIN32
    ConfigApp.cpp
)

target_link_libraries(macroman_config PRIVATE
    macroman_core   # For SharedConfig, SharedConfigManager, Logger
    imgui           # Dear ImGui
    d3d11.lib       # DirectX 11
    dxgi.lib        # DXGI for swap chain
    dwmapi.lib      # Desktop Window Manager
)

target_include_directories(macroman_config PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Copy to bin directory for easier execution
set_target_properties(macroman_config PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
