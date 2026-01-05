# Code Coverage Configuration for MacroMan AI Aimbot
#
# This module provides code coverage support for Windows (MSVC) using OpenCppCoverage.
#
# Usage:
#   include(cmake/CodeCoverage.cmake)
#   add_code_coverage_target(TARGET unit_tests OUTPUT_DIR coverage)

# Option to enable code coverage
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    if(MSVC)
        # For MSVC, we use OpenCppCoverage (requires installation)
        # Download: https://github.com/OpenCppCoverage/OpenCppCoverage/releases

        find_program(OPENCPPCOVERAGE_EXECUTABLE
            NAMES OpenCppCoverage
            PATHS
                "C:/Program Files/OpenCppCoverage"
                "C:/Program Files (x86)/OpenCppCoverage"
            DOC "Path to OpenCppCoverage executable"
        )

        if(NOT OPENCPPCOVERAGE_EXECUTABLE)
            message(WARNING "OpenCppCoverage not found. Coverage reports will not be generated.")
            message(STATUS "Download from: https://github.com/OpenCppCoverage/OpenCppCoverage/releases")
            set(ENABLE_COVERAGE OFF)
        else()
            message(STATUS "OpenCppCoverage found: ${OPENCPPCOVERAGE_EXECUTABLE}")

            # Function to add coverage target for a test executable
            function(add_code_coverage_target)
                set(options "")
                set(oneValueArgs TARGET OUTPUT_DIR)
                set(multiValueArgs SOURCES)
                cmake_parse_arguments(COVERAGE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

                if(NOT COVERAGE_TARGET)
                    message(FATAL_ERROR "add_code_coverage_target: TARGET argument required")
                endif()

                if(NOT COVERAGE_OUTPUT_DIR)
                    set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage")
                endif()

                # Create output directory
                file(MAKE_DIRECTORY "${COVERAGE_OUTPUT_DIR}")

                # Add custom target to run tests with coverage
                add_custom_target(coverage_${COVERAGE_TARGET}
                    COMMAND ${OPENCPPCOVERAGE_EXECUTABLE}
                        --sources ${CMAKE_SOURCE_DIR}/src
                        --sources ${CMAKE_SOURCE_DIR}/tests
                        --export_type html:${COVERAGE_OUTPUT_DIR}
                        --export_type cobertura:${COVERAGE_OUTPUT_DIR}/coverage.xml
                        --excluded_sources ${CMAKE_SOURCE_DIR}/build
                        --excluded_sources ${CMAKE_SOURCE_DIR}/external
                        --excluded_sources ${CMAKE_SOURCE_DIR}/modules
                        --excluded_line_regex ".*assert.*"
                        --excluded_line_regex ".*LOG_.*"
                        -- $<TARGET_FILE:${COVERAGE_TARGET}>
                    DEPENDS ${COVERAGE_TARGET}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    COMMENT "Running code coverage for ${COVERAGE_TARGET}"
                    VERBATIM
                )

                message(STATUS "Added coverage target: coverage_${COVERAGE_TARGET}")
                message(STATUS "  Output directory: ${COVERAGE_OUTPUT_DIR}")
                message(STATUS "  Run with: cmake --build build --target coverage_${COVERAGE_TARGET}")
            endfunction()
        endif()
    else()
        # For GCC/Clang, use gcov/lcov
        message(STATUS "Coverage enabled for GCC/Clang (gcov)")

        # Add compiler flags for coverage
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

        # Function for GCC/Clang coverage
        function(add_code_coverage_target)
            set(options "")
            set(oneValueArgs TARGET OUTPUT_DIR)
            set(multiValueArgs "")
            cmake_parse_arguments(COVERAGE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

            if(NOT COVERAGE_OUTPUT_DIR)
                set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage")
            endif()

            find_program(LCOV_EXECUTABLE lcov)
            find_program(GENHTML_EXECUTABLE genhtml)

            if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
                add_custom_target(coverage_${COVERAGE_TARGET}
                    COMMAND ${LCOV_EXECUTABLE} --directory . --zerocounters
                    COMMAND $<TARGET_FILE:${COVERAGE_TARGET}>
                    COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
                    COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' '*/build/*' '*/external/*' --output-file coverage.info.cleaned
                    COMMAND ${GENHTML_EXECUTABLE} -o ${COVERAGE_OUTPUT_DIR} coverage.info.cleaned
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    DEPENDS ${COVERAGE_TARGET}
                    COMMENT "Generating code coverage report"
                )
            else()
                message(WARNING "lcov/genhtml not found. Coverage reports will not be generated.")
            endif()
        endfunction()
    endif()
else()
    message(STATUS "Code coverage disabled (use -DENABLE_COVERAGE=ON to enable)")
endif()
