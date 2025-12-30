# Unit tests

# Test executable
add_executable(unit_tests
    test_latest_frame_queue.cpp
)

# Link libraries
target_link_libraries(unit_tests PRIVATE
    aimbot_core
    Catch2::Catch2WithMain
)

# Include directories
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)

# Add to CTest
include(CTest)
include(Catch)
catch_discover_tests(unit_tests)
