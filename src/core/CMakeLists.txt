# Core library

# Fetch spdlog (if not already done at root level, this is redundant but safe)
include(FetchContent)
if(NOT TARGET spdlog::spdlog)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
    )
    FetchContent_MakeAvailable(spdlog)
endif()

add_library(macroman_core
    threading/ThreadManager.cpp
    utils/Logger.cpp
    entities/TexturePool.cpp
)

target_include_directories(macroman_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/entities
    ${CMAKE_CURRENT_SOURCE_DIR}/threading
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Link libraries
target_link_libraries(macroman_core PUBLIC
    spdlog::spdlog
    d3d11.lib
    dxgi.lib
)

# C++20 features required
target_compile_features(macroman_core PUBLIC cxx_std_20)

# Platform-specific requirements
if(WIN32)
    target_compile_definitions(macroman_core PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()
