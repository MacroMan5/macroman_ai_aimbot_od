file(GLOB_RECURSE SOURCES "*.cpp") # Check if any
file(GLOB_RECURSE HEADERS "*.h")

add_library(aimbot_config STATIC ${SOURCES} ${HEADERS})

target_link_libraries(aimbot_config PUBLIC aimbot_core aimbot_input aimbot_detection)
