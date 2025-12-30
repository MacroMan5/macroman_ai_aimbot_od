file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_library(aimbot_core STATIC ${SOURCES} ${HEADERS})

target_include_directories(aimbot_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/.. # Allow including "core/..."
)

target_link_libraries(aimbot_core PUBLIC 
    OpenCV::opencv_core
    d3d11.lib
)
