# Collect driver sources
file(GLOB DRIVER_SOURCES
    "drivers/Win32Driver.cpp"
)

# Conditionally add ArduinoDriver if serial library is available
# Default: OFF (requires Arduino Leonardo/Pro Micro hardware + libserial library)
# To enable: cmake -DENABLE_ARDUINO=ON (see docs/ARDUINO_SETUP.md)
option(ENABLE_ARDUINO "Enable Arduino HID driver (requires libserial)" OFF)
if(ENABLE_ARDUINO)
    find_package(serial QUIET)
    if(serial_FOUND)
        list(APPEND DRIVER_SOURCES "drivers/ArduinoDriver.cpp")
        message(STATUS "ArduinoDriver enabled (libserial found)")
    else()
        message(WARNING "ENABLE_ARDUINO=ON but libserial not found. Install via: vcpkg install serial")
        message(WARNING "ArduinoDriver will be excluded from build.")
    endif()
else()
    message(STATUS "ArduinoDriver disabled (ENABLE_ARDUINO=OFF). To enable: cmake -DENABLE_ARDUINO=ON")
endif()
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
)

# Link serial library if ArduinoDriver is enabled
if(ENABLE_ARDUINO AND serial_FOUND)
    target_link_libraries(macroman_input PRIVATE serial)
endif()

target_include_directories(macroman_input PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Tracy profiler integration (optional)
if(ENABLE_TRACY)
    target_link_libraries(macroman_input PUBLIC Tracy::TracyClient)
endif()
