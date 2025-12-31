# Integration tests

# Integration test executable
add_executable(integration_tests
    test_pipeline_integration.cpp
    ../helpers/FakeScreenCapture.cpp
    ../helpers/FakeDetector.cpp
)

# Link libraries
target_link_libraries(integration_tests PRIVATE
    macroman_core
    macroman_detection
    macroman_tracking
    macroman_input
    macroman_ui
    imgui
    Catch2::Catch2WithMain
    d3d11.lib
    dxgi.lib
)

# Include directories
target_include_directories(integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/detection
    ${CMAKE_SOURCE_DIR}/src/input
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/tests
)

# Add to CTest
include(CTest)
include(Catch)
catch_discover_tests(integration_tests)

# Code coverage target (if enabled)
if(ENABLE_COVERAGE AND COMMAND add_code_coverage_target)
    add_code_coverage_target(
        TARGET integration_tests
        OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage/integration"
    )
endif()
