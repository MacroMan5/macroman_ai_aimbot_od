file(GLOB_RECURSE SOURCES "*.cpp")
file(GLOB_RECURSE HEADERS "*.h")

add_library(aimbot_input STATIC ${SOURCES} ${HEADERS})

target_link_libraries(aimbot_input PUBLIC aimbot_core serial)
