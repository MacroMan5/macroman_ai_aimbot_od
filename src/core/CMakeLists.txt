file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_library(aimbot_core STATIC 
    ${SOURCES} 
    ${HEADERS}
    threading/ThreadManager.cpp
    utils/Logger.cpp
)

target_include_directories(aimbot_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}    # Allow including "threading/...", "utils/..."
    ${CMAKE_CURRENT_SOURCE_DIR}/.. # Allow including "core/..."
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(aimbot_core PUBLIC 
    ${OpenCV_LIBRARIES}
    spdlog::spdlog
    Eigen3::Eigen
    d3d11.lib
)
