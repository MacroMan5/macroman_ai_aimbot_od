# Phase 7: Benchmark Tool Configuration
# CLI benchmark tool for performance regression testing in CI/CD pipelines

add_executable(macroman-bench
    macroman-bench.cpp
    ../helpers/FakeScreenCapture.cpp
    ../helpers/FakeDetector.cpp
)

# Add include directories (for helpers/ access)
target_include_directories(macroman-bench PRIVATE
    ${CMAKE_SOURCE_DIR}/tests  # For helpers/FakeScreenCapture.h access
)

# Link against core and tracking libraries
target_link_libraries(macroman-bench PRIVATE
    macroman_core
    macroman_tracking
)

# Windows high-resolution timer
if(WIN32)
    target_link_libraries(macroman-bench PRIVATE winmm)
endif()

# Set output directory
set_target_properties(macroman-bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
install(TARGETS macroman-bench
    RUNTIME DESTINATION bin
)
