cmake_minimum_required(VERSION 3.20)

project(MacromanAimbot VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Options ---
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_TENSORRT "Enable TensorRT backend" ON)

# --- Output Directories ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Compiler Settings ---
if(MSVC)
    add_compile_options(/W4 /MP /permissive- /Zc:__cplusplus)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Dependencies Management ---
include(FetchContent)

# 1. spdlog (Logging)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
)
FetchContent_MakeAvailable(spdlog)

# 2. Catch2 (Testing)
if(BUILD_TESTS)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.7.0
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# 3. Eigen (Math) - Disable Eigen's tests (we only need the library)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_BLAS OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_LAPACK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  Eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4.0
)
FetchContent_MakeAvailable(Eigen)
set(BUILD_TESTING ON CACHE BOOL "" FORCE)  # Re-enable for our tests

# 4. OpenCV (Optional - minimal usage in Phase 2)
if(NOT OpenCV_DIR)
    set(OpenCV_DIR "${CMAKE_SOURCE_DIR}/external/opencv/build")
endif()
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
else()
    message(WARNING "OpenCV not found - some features may be unavailable")
endif()

# 5. ImGui (Fetch from GitHub) - ENABLED FOR PHASE 6 (UI & Observability)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.2
)
# Use Populate instead of MakeAvailable since ImGui has no CMakeLists.txt
FetchContent_Populate(imgui)
FetchContent_GetProperties(imgui)

# ImGui doesn't provide CMakeLists.txt, so we build it manually
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PRIVATE d3d11.lib d3dcompiler.lib dwmapi.lib)

# 6. SimpleIni - DISABLED FOR PHASE 2 (Config comes in Phase 5)
# add_library(simpleini INTERFACE)
# target_include_directories(simpleini INTERFACE "external/simpleini")

# 7. GLFW (Imported) - DISABLED FOR PHASE 2 (UI comes in Phase 5+)
# add_library(glfw STATIC IMPORTED)
# set_target_properties(glfw PROPERTIES
#     IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022/glfw3.lib"
#     INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/external/glfw/include"
# )

# 8. Serial (Manual build to avoid catkin dependency) - DISABLED FOR PHASE 2 (Input comes in Phase 4)
# set(SERIAL_SOURCES
#     "external/serial/src/serial.cc"
#     "external/serial/src/impl/win.cc"
#     "external/serial/src/impl/list_ports/list_ports_win.cc"
# )
# add_library(serial STATIC ${SERIAL_SOURCES})
# target_include_directories(serial PUBLIC "external/serial/include")
# target_link_libraries(serial PRIVATE setupapi)

# 9. ONNX Runtime (Detected from NuGet or provided via -DONNXRUNTIME_ROOT)
if(NOT ONNXRUNTIME_ROOT)
    set(ONNXRUNTIME_ROOT "C:/Users/therouxe/.nuget/packages/microsoft.ml.onnxruntime.directml/1.22.0" CACHE PATH "Path to ONNX Runtime root")
endif()

# Fallback check
if(NOT EXISTS "${ONNXRUNTIME_ROOT}")
    message(WARNING "ONNX Runtime not found at ${ONNXRUNTIME_ROOT}. Please specify -DONNXRUNTIME_ROOT=<path>")
endif()

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime.dll"
    IMPORTED_IMPLIB "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native/onnxruntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_ROOT}/build/native/include"
)

# 10. TensorRT (NVIDIA-only, conditional)
if(ENABLE_TENSORRT)
    find_package(CUDAToolkit 12.0 REQUIRED)
    find_package(TensorRT REQUIRED)
    message(STATUS "TensorRT enabled: ${TensorRT_VERSION}")
endif()

# Global include directory
include_directories("${CMAKE_SOURCE_DIR}/src")

# --- Subdirectories ---
add_subdirectory(src)

# --- Tests ---
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Assets Copy ---
add_custom_target(CopyAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
    COMMENT "Copying assets to output directory"
)
