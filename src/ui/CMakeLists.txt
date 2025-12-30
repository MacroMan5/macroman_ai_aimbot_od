file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_library(aimbot_ui STATIC ${SOURCES} ${HEADERS})

target_link_libraries(aimbot_ui PUBLIC 
    aimbot_core 
    imgui 
    d3d11.lib 
    dxgi.lib
)
