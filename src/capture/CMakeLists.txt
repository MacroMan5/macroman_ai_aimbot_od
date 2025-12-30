# Capture module - WinRT and Desktop Duplication

add_library(macroman_capture
    DuplicationCapture.cpp
)

target_include_directories(macroman_capture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
)

# Link core library
target_link_libraries(macroman_capture PUBLIC
    macroman_core
    d3d11.lib
    dxgi.lib
)

# Windows Runtime (WinRT) support
target_compile_definitions(macroman_capture PRIVATE
    WINVER=0x0A00          # Windows 10
    _WIN32_WINNT=0x0A00    # Windows 10
)

# C++/WinRT support (for Windows.Graphics.Capture)
target_compile_options(macroman_capture PRIVATE
    /await  # Enable coroutines (required for WinRT)
)

# C++20 features
target_compile_features(macroman_capture PUBLIC cxx_std_20)
