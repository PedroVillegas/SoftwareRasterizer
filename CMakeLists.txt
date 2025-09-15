cmake_minimum_required(VERSION 4.0)
project(Grace_Software_Rasterizer)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
    glfw
    EXCLUDE_FROM_ALL
    DOWNLOAD_EXTRACT_TIMESTAMP true
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        f0569113c93ad095470c54bf34a17b36646bbbb5 # Release - ???
)

FetchContent_Declare(
    glm
    DOWNLOAD_EXTRACT_TIMESTAMP true
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
)

FetchContent_Declare(
    fastgltf
    DOWNLOAD_EXTRACT_TIMESTAMP true
    URL https://github.com/spnda/fastgltf/archive/refs/tags/v0.9.0.zip
)

FetchContent_Declare(
    spdlog
    DOWNLOAD_EXTRACT_TIMESTAMP true
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.zip
)

FetchContent_MakeAvailable(glfw stb glm fastgltf spdlog)

set(GRACE_SPIRV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Resources/SPIRV/)
set(GRACE_SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shaders/)
set(GRACE_CUSTOM_SLANG_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/Bin/slang-2025.13/bin/slangc.exe)
set(GRACE_TARGET_VULKAN_API_VERSION 13)
set(GRACE_DESIRED_VULKAN_SDK_VERSION "1.3.296.0")

add_library(Grace SHARED)
add_subdirectory(Third-Party/Grace)

add_executable(Grace_Software_Rasterizer)

target_sources(Grace_Software_Rasterizer PRIVATE
    Source/Main.cpp
    Source/Camera.cpp
    Source/GltfLoader.cpp
)

target_compile_definitions(Grace_Software_Rasterizer
    PRIVATE GRACE_USE_GLFW
    PRIVATE RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Resources/"
)

target_link_libraries(Grace_Software_Rasterizer PRIVATE Grace glfw glm fastgltf spdlog)

target_include_directories(Grace_Software_Rasterizer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/
    ${stb_SOURCE_DIR}
)

