# macroman_ui library (overlay components only - excludes config_app executable)
file(GLOB_RECURSE UI_SOURCES "overlay/*.cpp" "backend/*.cpp")
file(GLOB_RECURSE UI_HEADERS "overlay/*.h" "backend/*.h")

add_library(macroman_ui STATIC ${UI_SOURCES} ${UI_HEADERS})

target_include_directories(macroman_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(macroman_ui PUBLIC
    macroman_core
    imgui
    d3d11.lib
    dxgi.lib
)

# External Config UI Application (standalone executable)
add_subdirectory(config_app)
