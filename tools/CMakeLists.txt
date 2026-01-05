# Phase 7: Tools Configuration
# Utility tools for dataset recording and analysis

add_executable(dataset-recorder
    dataset-recorder.cpp
)

# Link against core libraries
target_link_libraries(dataset-recorder PRIVATE
    macroman_core
)

# Set output directory
set_target_properties(dataset-recorder PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
install(TARGETS dataset-recorder
    RUNTIME DESTINATION bin
)
