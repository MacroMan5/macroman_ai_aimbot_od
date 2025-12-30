file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(aimbot_tracking STATIC ${SOURCES} ${HEADERS})

target_link_libraries(aimbot_tracking PUBLIC aimbot_core)
