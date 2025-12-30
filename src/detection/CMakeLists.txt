# Detection module - DirectML and TensorRT backends

# Find ONNX Runtime (using NuGet package location from root CMake)
set(ONNXRUNTIME_ROOT "C:/Users/therouxe/.nuget/packages/microsoft.ml.onnxruntime.directml/1.22.0")
set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/build/native/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native")

# Check if ONNX Runtime exists
if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime not found at ${ONNXRUNTIME_ROOT}. Please install it via NuGet.")
endif()

add_library(macroman_detection
    directml/InputPreprocessor.cpp
    # PostProcessor added later
)

target_include_directories(macroman_detection PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(macroman_detection PUBLIC
    macroman_core
    d3d11.lib
    d3dcompiler.lib # For shader compilation if needed, or runtime loading
    "${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib"
)

# Copy ONNX Runtime DLL to output directory
add_custom_command(TARGET macroman_detection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll"
        "$<TARGET_FILE_DIR:macroman_detection>"
    COMMENT "Copying onnxruntime.dll to output directory"
)

# Compile HLSL Shader
# Requires 'fxc' or 'dxc' available in path, or use VS rules.
# For CMake cross-platform simple approach, we might skip automatic compilation
# and expect .cso to be present, or use a custom command.
find_program(FXC_EXECUTABLE fxc)
if(FXC_EXECUTABLE)
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/InputPreprocessing.cso"
        COMMAND ${FXC_EXECUTABLE} /T cs_5_0 /E CSMain /Fo "${CMAKE_BINARY_DIR}/InputPreprocessing.cso" "${CMAKE_CURRENT_SOURCE_DIR}/directml/InputPreprocessing.hlsl"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/directml/InputPreprocessing.hlsl"
        COMMENT "Compiling Compute Shader"
    )
    add_custom_target(CompileShaders ALL DEPENDS "${CMAKE_BINARY_DIR}/InputPreprocessing.cso")
    add_dependencies(macroman_detection CompileShaders)
else()
    message(WARNING "fxc.exe not found. Shaders will not be compiled.")
endif()

# C++20 features
target_compile_features(macroman_detection PUBLIC cxx_std_20)
