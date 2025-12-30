file(GLOB SOURCES "*.cpp" "postprocess/*.cpp")
file(GLOB HEADERS "*.h" "postprocess/*.h")

# DirectML Backend
file(GLOB DML_SOURCES "directml/*.cpp")
file(GLOB DML_HEADERS "directml/*.h")

add_library(aimbot_detection STATIC ${SOURCES} ${HEADERS} ${DML_SOURCES} ${DML_HEADERS})

target_include_directories(aimbot_detection PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/directml
    ${CMAKE_CURRENT_SOURCE_DIR}/tensorrt
    ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(aimbot_detection PUBLIC 
    aimbot_core 
    ${OpenCV_LIBRARIES}
    onnxruntime
    d3d11.lib 
    d3dcompiler.lib
)

if(USE_CUDA)
    file(GLOB TRT_SOURCES "tensorrt/*.cpp")
    file(GLOB TRT_HEADERS "tensorrt/*.h")
    # Note: .cu handling requires enable_language(CUDA) in root
    target_sources(aimbot_detection PRIVATE ${TRT_SOURCES} ${TRT_HEADERS})
endif()
