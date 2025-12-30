# Collect source files (excluding ArduinoDriver for now - serial lib not set up)
file(GLOB DRIVER_SOURCES
    "drivers/Win32Driver.cpp"
)
file(GLOB MOVEMENT_SOURCES
    "movement/*.cpp"
)
file(GLOB HUMANIZATION_SOURCES
    "humanization/*.cpp"
)

# Orchestrator
set(MANAGER_SOURCES
    "InputManager.cpp"
)

# Collect all headers
file(GLOB_RECURSE HEADERS "*.h")

add_library(macroman_input STATIC
    ${DRIVER_SOURCES}
    ${MOVEMENT_SOURCES}
    ${HUMANIZATION_SOURCES}
    ${MANAGER_SOURCES}
    ${HEADERS}
)

target_link_libraries(macroman_input PUBLIC 
    macroman_core 
    opencv_world
)

target_include_directories(macroman_input PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)
