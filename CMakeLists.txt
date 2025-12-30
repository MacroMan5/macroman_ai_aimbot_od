cmake_minimum_required(VERSION 3.20)

project(MacromanAimbot VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Options ---
option(USE_CUDA "Enable TensorRT/CUDA support" OFF)
option(BUILD_TESTS "Build unit tests" ON)

# --- Output Directories ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Compiler Settings ---
if(MSVC)
    add_compile_options(/W4 /MP /permissive- /Zc:__cplusplus)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

# --- Module Path ---
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# --- Dependencies ---

# 1. OpenCV
# Assuming external/opencv/build exists. If not, user must provide OpenCV_DIR.
if(NOT OpenCV_DIR)
    set(OpenCV_DIR "${CMAKE_SOURCE_DIR}/external/opencv/build")
endif()
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV found: ${OpenCV_VERSION}")

# 2. ImGui (Build from source)
# We will create a library target for ImGui
file(GLOB IMGUI_SOURCES 
    "external/imgui/*.cpp" 
    "external/imgui/backends/imgui_impl_win32.cpp"
    "external/imgui/backends/imgui_impl_dx11.cpp"
)
file(GLOB IMGUI_HEADERS 
    "external/imgui/*.h"
    "external/imgui/backends/imgui_impl_win32.h"
    "external/imgui/backends/imgui_impl_dx11.h"
)

add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})
target_include_directories(imgui PUBLIC "external/imgui" "external/imgui/backends")
target_link_libraries(imgui PRIVATE d3d11.lib d3dcompiler.lib dwmapi.lib)

# 3. SimpleIni
add_library(simpleini INTERFACE)
target_include_directories(simpleini INTERFACE "external/simpleini")

# 4. GLFW (Imported)
add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/external/glfw/include"
)

# 5. Serial (Build from source)
# Assuming external/serial has a CMakeLists.txt, otherwise we build it here
if(EXISTS "${CMAKE_SOURCE_DIR}/external/serial/CMakeLists.txt")
    add_subdirectory(external/serial)
else()
    # Fallback if serial CMakeLists is missing or complex
    file(GLOB SERIAL_SRCS "external/serial/src/*.cc" "external/serial/src/impl/win.cc")
    add_library(serial STATIC ${SERIAL_SRCS})
    target_include_directories(serial PUBLIC "external/serial/include")
    target_link_libraries(serial PRIVATE setupapi)
endif()

# 6. ONNX Runtime & DirectML
# We expect these to be provided via NuGet or manual install. 
# For now, we define INTERFACE targets that point to headers if found, 
# or error out if critical paths are missing in a real build.
# For Phase 0 scaffold, we'll assume they are in system path or user provides them.

find_path(ONNXRUNTIME_INCLUDE_DIR NAMES onnxruntime_cxx_api.h PATHS 
    "${CMAKE_SOURCE_DIR}/external/onnxruntime/include"
    "C:/Program Files/onnxruntime/include"
)
# TODO: Find libraries

# --- Subdirectories ---
add_subdirectory(src)

# --- Assets Copy ---
# Copy assets to bin/assets on build
add_custom_target(CopyAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
    COMMENT "Copying assets to output directory"
)