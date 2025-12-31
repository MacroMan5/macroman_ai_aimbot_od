# Unit tests

# Test executable
add_executable(unit_tests
    test_latest_frame_queue.cpp
    test_texture_pool.cpp
    test_post_processor.cpp
    test_math_types.cpp
    test_detection_batch.cpp
    test_target_database.cpp
    test_data_association.cpp
    test_kalman_predictor.cpp
    test_target_selector.cpp
    test_aim_command.cpp
    test_target_tracker.cpp
    test_input_humanization.cpp
    test_input_integration.cpp
    test_profile_manager.cpp
)

# Link libraries
target_link_libraries(unit_tests PRIVATE
    macroman_core
    macroman_detection
    macroman_tracking
    macroman_input
    Catch2::Catch2WithMain
    d3d11.lib
    dxgi.lib
)

# Include directories
target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/detection
    ${CMAKE_SOURCE_DIR}/src/input
)

# Add to CTest
include(CTest)
include(Catch)
catch_discover_tests(unit_tests)
