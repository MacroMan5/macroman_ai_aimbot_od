# Detection module - DirectML and TensorRT backends

# Core detection sources
set(DETECTION_SOURCES
    directml/InputPreprocessor.cpp
    postprocess/PostProcessor.cpp
)

# DirectML detector (default - works on all GPUs)
list(APPEND DETECTION_SOURCES
    directml/DMLDetector.cpp
)

# TensorRT detector (NVIDIA-only, conditional)
if(ENABLE_TENSORRT)
    list(APPEND DETECTION_SOURCES
        tensorrt/TensorRTDetector.cpp
    )
    add_compile_definitions(USE_CUDA)
    message(STATUS "TensorRT detector enabled")
endif()

add_library(macroman_detection STATIC ${DETECTION_SOURCES})

target_include_directories(macroman_detection PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
)

# Link libraries
target_link_libraries(macroman_detection PUBLIC
    macroman_core
    onnxruntime
    d3d11.lib
    d3dcompiler.lib # For shader compilation if needed, or runtime loading
)

# Conditional TensorRT dependencies
if(ENABLE_TENSORRT)
    target_link_libraries(macroman_detection PRIVATE
        TensorRT::nvinfer
        TensorRT::nvonnxparser
        CUDA::cudart
    )
endif()

# Copy ONNX Runtime DLL to output directory
# We can use the IMPORTED_LOCATION property from the onnxruntime target
get_target_property(ORT_DLL onnxruntime IMPORTED_LOCATION)
if(ORT_DLL)
    add_custom_command(TARGET macroman_detection POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ORT_DLL}"
            "$<TARGET_FILE_DIR:macroman_detection>"
        COMMENT "Copying onnxruntime.dll to output directory"
    )
endif()

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
