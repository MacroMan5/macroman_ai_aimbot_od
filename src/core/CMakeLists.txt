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

# Fetch nlohmann/json
if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(macroman_core
    threading/ThreadManager.cpp
    utils/Logger.cpp
    entities/TexturePool.cpp
    config/ProfileManager.cpp
    config/GameDetector.cpp
    config/ModelManager.cpp
    config/SharedConfigManager.cpp
)

target_include_directories(macroman_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/entities
    ${CMAKE_CURRENT_SOURCE_DIR}/threading
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)

# Link libraries
target_link_libraries(macroman_core PUBLIC
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    d3d11.lib
    dxgi.lib
    Psapi.lib
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
