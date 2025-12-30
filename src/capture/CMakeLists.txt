file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

add_library(aimbot_capture STATIC ${SOURCES} ${HEADERS})

target_link_libraries(aimbot_capture PUBLIC 
    aimbot_core
    d3d11.lib
    dxgi.lib
)

if(MSVC)
    # Link against WindowsApp.lib for WinRT APIs
    target_link_libraries(aimbot_capture PRIVATE windowsapp)
    # Enable C++/WinRT
    target_compile_options(aimbot_capture PRIVATE /std:c++20 /await) 
endif()
