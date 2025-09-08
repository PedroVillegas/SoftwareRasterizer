cmake_minimum_required(VERSION 3.25)

project(Grace VERSION 1.0.0)

message("Building Grace...")

include(FetchContent)

FetchContent_Declare(
        vma
        DOWNLOAD_EXTRACT_TIMESTAMP true
        URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.zip
)
FetchContent_MakeAvailable(vma)

if (NOT DEFINED GRACE_TARGET_VULKAN_API_VERSION)
    message(WARNING "[GRACE_WARN] GRACE_TARGET_VULKAN_API_VERSION was not defined - default is 1.3!")
    set(GRACE_TARGET_VULKAN_API_VERSION 13)
endif()

if(NOT DEFINED GRACE_SHADERS_DIR)
    message(FATAL_ERROR "[GRACE_ERROR] GRACE_SHADERS_DIR must be defined!")
endif()

if(NOT DEFINED GRACE_SPIRV_DIR)
    message(WARNING "[GRACE_WARN] GRACE_SPIRV_DIR was not defined - falling back to default!")
    set(GRACE_SPIRV_DIR ${GRACE_SHADERS_DIR}/../SPIRV)
endif()

if (NOT DEFINED GRACE_DESIRED_VULKAN_SDK_VERSION)
    find_package(Vulkan REQUIRED)
else()
    find_package(Vulkan ${GRACE_DESIRED_VULKAN_SDK_VERSION} REQUIRED)
    if (NOT ${Vulkan_FOUND})
        message(FATAL_ERROR "[GRACE_ERROR] Could not find specified Vulkan SDK version (${GRACE_DESIRED_VULKAN_SDK_VERSION})")
    endif()
endif()

set(Vulkan_BIN_DIR ${Vulkan_INCLUDE_DIR}/../Bin)

if (Vulkan_FOUND)
    message("-- Found Vulkan SDK: ${Vulkan_VERSION}")
endif()

if (NOT DEFINED GRACE_CUSTOM_GLSL_COMPILER)
    find_program(GLSL_COMPILER glslc HINTS Vulkan_BIN_DIR)
    if (EXISTS ${GLSL_COMPILER})
        message("-- Found GLSL to SPIRV compiler (${GLSL_COMPILER})")
    endif()
else()
    set(GLSL_COMPILER ${GRACE_CUSTOM_GLSL_COMPILER})
    message("-- Detected path to custom GLSL compiler (${GLSL_COMPILER})")
endif()

if (NOT DEFINED GRACE_CUSTOM_HLSL_COMPILER)
    find_program(HLSL_COMPILER dxc HINTS Vulkan_BIN_DIR)
    if (EXISTS ${HLSL_COMPILER})
        message("-- Found HLSL to SPIRV compiler (${HLSL_COMPILER})")
    endif()
else()
    set(HLSL_COMPILER ${GRACE_CUSTOM_HLSL_COMPILER})
    message("-- Detected path to custom HLSL compiler (${HLSL_COMPILER})")
endif()

if (NOT DEFINED GRACE_CUSTOM_SLANG_COMPILER)
    find_program(SLANG_COMPILER slangc HINTS Vulkan_BIN_DIR)
    if (EXISTS ${SLANG_COMPILER})
        message("-- Found SLANG to SPIRV compiler (${SLANG_COMPILER})")
    endif()
else()
    set(SLANG_COMPILER ${GRACE_CUSTOM_SLANG_COMPILER})
    message("-- Detected path to custom SLANG compiler (${SLANG_COMPILER})")
endif()

set(
    GRACE_LIB_SOURCE
    Grace/Buffer.cpp
    Grace/CommandGroup.cpp
    Grace/Context.cpp
    Grace/DebugReporter.cpp
    Grace/DescriptorGroup.cpp
    Grace/Device.cpp
    Grace/DeletionQueue.cpp
    Grace/GpuResourceTable.cpp
    Grace/HelperFunctions.cpp
    Grace/Image.cpp
    Grace/PipelineGroup.cpp
    Grace/ResourceManager.cpp
    Grace/Sampler.cpp
    Grace/Swapchain.cpp
    Grace/SyncGroup.cpp
    Grace/QueryManager.cpp
    Grace/Fence.cpp
    Grace/Semaphore.cpp
    Grace/VmaUsage.cpp
)

target_sources(
    Grace
    PUBLIC ${GRACE_LIB_SOURCE}
)

target_compile_features(
    Grace
    PUBLIC cxx_std_20
)

set_target_properties(
    Grace PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    C_VISIBILITY_PRESET "hidden"
    CXX_VISIBILITY_PRESET "hidden"
    VISIBILITY_INLINES_HIDDEN TRUE
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

include(GenerateExportHeader)
generate_export_header(
    Grace
    EXPORT_FILE_NAME "${CMAKE_CURRENT_SOURCE_DIR}/Grace/GraceExport.h"
)

target_include_directories(
    Grace
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${Vulkan_INCLUDE_DIRS}
    PUBLIC ${vma_SOURCE_DIR}/include/
)

target_link_libraries(
    Grace
    PUBLIC ${Vulkan_LIBRARIES}
)

target_compile_definitions(
    Grace
    PUBLIC Grace_EXPORTS
    PUBLIC GRACE_SPIRV_DIR="${GRACE_SPIRV_DIR}"
    PUBLIC GRACE_TARGET_VULKAN_API_VERSION=${GRACE_TARGET_VULKAN_API_VERSION}
)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${GRACE_SHADERS_DIR}/*.frag"
    "${GRACE_SHADERS_DIR}/*.vert"
    "${GRACE_SHADERS_DIR}/*.comp"
)

file(GLOB_RECURSE HLSL_FILES
    "${GRACE_SHADERS_DIR}/*.hlsl"
)

file(GLOB_RECURSE SLANG_FILES
    "${GRACE_SHADERS_DIR}/*.slang"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${GRACE_SPIRV_DIR}/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_COMPILER} ${GLSL} -g -o ${SPIRV} --target-env=vulkan1.3
        DEPENDS ${GLSL}
        COMMENT "Compiling GLSL Shader: ${GLSL}"
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

foreach(HLSL ${HLSL_FILES})
    # Read top of file to determine shader stage
    file(STRINGS ${HLSL} SHADER_STAGE LIMIT_INPUT 32 REGEX "#pragma")
    if (${SHADER_STAGE} STREQUAL "#pragma compute")
        set(TARGET_PROFILE "cs_6_5")
    elseif(${SHADER_STAGE} STREQUAL "#pragma vertex")
        set(TARGET_PROFILE "vs_6_5")
    elseif(${SHADER_STAGE} STREQUAL "#pragma pixel")
        set(TARGET_PROFILE "ps_6_5")
    endif()

    get_filename_component(FILE_NAME ${HLSL} NAME)
    set(SPIRV "${GRACE_SPIRV_DIR}/${FILE_NAME}.spv")

    set(HLSL_COMPILE_CMD
        ${HLSL_COMPILER} ${HLSL}
        -spirv
        -HV 2021
        -T ${TARGET_PROFILE}
        -E "main"
        -Fo ${SPIRV}
        -fvk-use-scalar-layout
        -fspv-target-env=vulkan1.3
    )

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${HLSL_COMPILE_CMD}
        DEPENDS ${HLSL}
        COMMENT "Compiling HLSL Shader: ${HLSL}"
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(HLSL)

# spirv-opt generates invalid spirv with debug info, so debug info is disabled for now
#set(SLANG_COMPILE_DEBUG_INFO $<IF:$<CONFIG:Debug>,-g,-g0>)
#set(SLANG_COMPILE_OPT $<IF:$<CONFIG:Debug>,-O0,-O2>)

set(SLANG_COMPILE_DEBUG_INFO -g0)
set(SLANG_COMPILE_OPT -O2)

foreach(SLANG ${SLANG_FILES})
    file(STRINGS ${SLANG} PRAGMA LIMIT_INPUT 23 REGEX "#pragma")
    if ("${PRAGMA}" STREQUAL "#pragma DO_NOT_COMPILE")
        message("Ignoring ${SLANG}")
        continue()
    endif()

    get_filename_component(FILE_NAME ${SLANG} NAME)
    set(SPIRV "${GRACE_SPIRV_DIR}/${FILE_NAME}.spv")

    set(SLANG_COMPILE_CMD
        ${SLANG_COMPILER} ${SLANG}
        -target spirv
        -profile spirv_1_6
        -force-glsl-scalar-layout
        ${SLANG_COMPILE_OPT}
        ${SLANG_COMPILE_DEBUG_INFO}
        -o ${SPIRV}
        -entry main
    )

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${SLANG_COMPILE_CMD}
        DEPENDS ${SLANG}
        COMMENT "Compiling SLANG Shader: ${SLANG}"
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(SLANG)

add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})

add_dependencies(Grace Shaders)

message(" ")