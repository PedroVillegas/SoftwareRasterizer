#include <chrono>
#include <format>
#include <iostream>

#include <GLFW/glfw3.h>
#include <Grace/Grace.hpp>

#include <Camera.hpp>
#include <GltfLoader.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct LongLifetimeGlobalVars
{
    glm::uvec2 framebufferSize;
    uint32_t ultraCoarseRasterTileSize;
    uint32_t coarseRasterTileSize;
    uint32_t fineRasterTileSize;
    glm::uvec2 ultraCoarseRasterDispatchSize;
    glm::uvec2 coarseRasterDispatchSize;
    glm::uvec2 fineRasterDispatchSize;

    uint32_t clipperEmittedVerticesOffset;
};

struct GraphicsPipelineMetaData
{
    uint32_t compactedIntrinsicDataBufferSize;
    uint32_t clipperEmittedVerticesCount;
    uint32_t validClipperEmittedTrianglesCount;
};

struct GraphicsPipelineIntrinsicData
{
    glm::vec4 homogeneousPosition;
};

struct FrameData
{
    Grace::CommandPool* pCmdPool = nullptr;
    Grace::CommandBuffer cmd = {};
    Grace::FenceHandle inFlightFence = {};
};

int main()
{
    constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    // Timings
    double gpuFrameTime = 0.0;
    float cpuFrameTime = 0.0F;

    // Options
    const float cameraSpeed = 6.0F;
    const float cameraSensitivity = 10.0F;
    bool vsync = false;
    bool framebufferHasResized = false;
    uint32_t windowWidth = 800;
    uint32_t windowHeight = 600;

    constexpr uint32_t maxResolutionWidth = 1920;
    constexpr uint32_t maxResolutionHeight = 1080;
    constexpr uint32_t maxPrimitivesPerBinOrTile = 1 << 16;
    constexpr uint32_t primitiveBitmapBlockCount = 1024; //maxPrimitivesPerBinOrTile >> 5;
    constexpr uint32_t densityBitmapBlockCount = 1;
    constexpr uint32_t coarseRasterizerTileSize = 16;
    constexpr uint32_t binningRasterizerTileCount = 16; // count x count tiles in a single bin
    constexpr uint32_t binningRasterizerBinSize = 256;  // coarseRasterizerTileSize * binningRasterizerTileCount;
    constexpr uint32_t maxTrianglesEmittedByClipper = 4096;
    const uint32_t binningRasterizerTotalBinCount =
        glm::ceil(static_cast<float>(maxResolutionWidth) / binningRasterizerBinSize)
        * glm::ceil(static_cast<float>(maxResolutionHeight) / binningRasterizerBinSize);
    const size_t binningRasterizerBufferSize =
        binningRasterizerTotalBinCount * sizeof(uint32_t) * (densityBitmapBlockCount * (1 + primitiveBitmapBlockCount));

    uint32_t previousVertexCount = 0;

    LongLifetimeGlobalVars longLifetimeGlobalVars = {
        .framebufferSize = { windowWidth, windowHeight },
        .ultraCoarseRasterTileSize = 256,
        .coarseRasterTileSize = 64,
        .fineRasterTileSize = 16,
        .ultraCoarseRasterDispatchSize = { glm::floor(static_cast<float>(windowWidth) / 256) + 1,
                                           glm::floor(static_cast<float>(windowHeight) / 256) + 1 },
        .coarseRasterDispatchSize = { glm::floor(static_cast<float>(windowWidth) / 64) + 1,
                                      glm::floor(static_cast<float>(windowHeight) / 64) + 1 },
        .fineRasterDispatchSize = { glm::floor(static_cast<float>(windowWidth) / 16) + 1,
                                    glm::floor(static_cast<float>(windowHeight) / 16) + 1 },
        .clipperEmittedVerticesOffset = 12'500'000 - 4096,
    };

    glfwInit();
    const GLFWvidmode* vm = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_POSITION_X, (vm->width - windowWidth) / 2);
    glfwWindowHint(GLFW_POSITION_Y, (vm->height - windowHeight) / 2);
    GLFWwindow* pWindow = glfwCreateWindow(windowWidth, windowHeight, "Grace-Software-Rasterizer", nullptr, nullptr);
    glfwSetWindowUserPointer(pWindow, &framebufferHasResized);
    glfwSetFramebufferSizeCallback(pWindow,
                                   [](GLFWwindow* pWindow, int width, int height)
                                   {
                                       bool& self = *static_cast<bool*>(glfwGetWindowUserPointer(pWindow));
                                       self = true;
                                   });

    const Grace::DeviceDesc deviceDesc = {
        .maxImageDescriptors = 65535,
        .maxSamplerDescriptors = 65535,
        .maxBufferDescriptors = 65535,
        .framesInFlight = FRAMES_IN_FLIGHT,
        .queryGroupDesc = {
            .timestampQueriesCount = 8,
            .pipelineStatisticsFlags = VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
        },
        .pGlfwWindow = pWindow,
    };

    Grace::Context gpuContext({ .deviceConfig = deviceDesc });
    Grace::Device* pDevice = gpuContext.GetDevicePtr();

    VkPhysicalDeviceProperties2 pdp;
    pdp.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    pdp.pNext = nullptr;
    vkGetPhysicalDeviceProperties2(pDevice->GetPhysicalDevice(), &pdp);
    // std::cout << "\nmaxComputeSharedMemorySize: " << pdp.properties.limits.maxComputeSharedMemorySize;

    // Must first create the swapchain with desired extents
    pDevice->CreateSwapchain({ windowWidth, windowHeight }, vsync);

    std::array<FrameData, FRAMES_IN_FLIGHT> frame = {};
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        const std::string cmdPoolDebugName = "GSR::pCmdPool::" + std::to_string(i);
        Grace::CommandPool* pCmdPool = pDevice->GetCommandPool(Grace::QueueFamily::Graphics, cmdPoolDebugName.c_str());
        frame[i] = {
            .pCmdPool = pCmdPool,
            .cmd = pCmdPool->GetOrAllocateCommandBuffer(),
        };

        const std::string fenceDebugName = "GSR::inFlightFence::" + std::to_string(i);
        frame[i].inFlightFence = pDevice->CreateFence({
            .name = fenceDebugName.c_str(),
            .createFlags = VK_FENCE_CREATE_SIGNALED_BIT,
        });
    }

    const Grace::BufferHandle tileDataBuffer = pDevice->CreateBuffer({
        .name = "GSR::tileDataBuffer",
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = binningRasterizerBufferSize,
        .data = nullptr,
    });

    const Grace::BufferHandle metaDataBuffer = pDevice->CreateBuffer({
        .name = "GSR::metaDataBuffer",
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = sizeof(GraphicsPipelineMetaData),
        .data = nullptr,
    });

    const Grace::BufferHandle graphicsPipelineInstrinsicDataBuffer = pDevice->CreateBuffer({
        .name = "GSR::graphicsPipelineInstrinsicDataBuffer",
        .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = 12'500'000 * sizeof(GraphicsPipelineIntrinsicData), // 200MB
        .data = nullptr,
    });

    const Grace::BufferHandle compactedInstrinsicDataBuffer = pDevice->CreateBuffer({
        .name = "GSR::compactedInstrinsicDataBuffer",
        .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = 12'500'000 * sizeof(GraphicsPipelineIntrinsicData), // 200MB
        .data = nullptr,
    });

    Grace::BufferHandle longLifetimeGlobalVarsBuffer = pDevice->CreateBuffer({
        .name = "GSR::longLifetimeGlobalVarsBuffer",
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = sizeof(LongLifetimeGlobalVars),
        .data = &longLifetimeGlobalVars,
    });

    Grace::ImageHandle renderImg = pDevice->CreateImage({
        .name = "GSR::renderImg",
        .dimensions = { .width = windowWidth, .height = windowHeight, .depth = 1 },
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .access = Grace::AccessType::General,
        .size = 0,
        .data = nullptr,
        .mipmapped = false,
    });

    Grace::ImageHandle depthImg = pDevice->CreateImage({
        .name = "GSR::depthImg",
        .dimensions = { windowWidth, windowHeight, 1 },
        .format = VK_FORMAT_R32_SFLOAT,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT,
        .access = Grace::AccessType::General,
        .size = 0,
        .data = nullptr,
        .mipmapped = false,
    });

    Grace::PipelineBuilder pbuilder(pDevice);
    pbuilder.AddShader("RenderImageClear.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::mainPipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle mainPipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("VertexStage.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::vertexStagePipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle vertexStagePipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("UltraCoarseRasterizer.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::ultraCoarseRasterizerPipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle ultraCoarseRasterizerPipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("UltraCoarseRasterizerDensityBitmap.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::ultraCoarseRasterizerDensityBitmap", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle ultraCoarseRasterizerDensityBitmapPipeline =
        pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("FineRasterizer.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::fineRasterizerPipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle fineRasterizerPipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("PrimitiveAssembly.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::primitiveAsmPipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle primitiveAsmPipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("NativePrimitivesCompaction.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::nativePrimitivesCompactionPipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle nativePrimitivesCompactionPipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("ClipperEmittedPrimitivesCompaction.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::clipperEmittedPrimitivesCompaction", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle clipperEmittedPrimitivesCompactionPipeline =
        pDevice->CreatePipeline(pbuilder.pipelineDesc);

    // std::optional<Scene> testScene = LoadGltf(RESOURCES_PATH "Models/damaged-helmet/damagedHelmet.gltf");
    // std::optional<Scene> testScene = LoadGltf(RESOURCES_PATH "Models/Key.glb");
    std::optional<Scene> testScene = LoadGltf(RESOURCES_PATH "Models/Rock.glb");
    assert(testScene.has_value());

    const Grace::BufferHandle vertexBuffer = pDevice->CreateBuffer({
        .name = "GSR::vertexBuffer",
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = testScene.value().vertices.size() * sizeof(Vertex),
        .data = testScene.value().vertices.data(),
    });

    const Grace::BufferHandle indexBuffer = pDevice->CreateBuffer({
        .name = "GSR::indexBuffer",
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = testScene.value().indices.size() * sizeof(uint32_t),
        .data = testScene.value().indices.data(),
    });

    // MVP
    Camera cam = {};
    // cam.position = { 1.5F, 1.5F, -3.0F };
    // cam.rotation = { -25.0F, 25.0F, 0.0F };
    cam.position = { -5.53F, 4.01F, 43.21F };
    cam.rotation = { -20.35F, 331.95F, 0.0F };

    // glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -6.0f));
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(100.0F));
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::perspectiveFov(
        glm::radians(45.0f), static_cast<float>(windowWidth), static_cast<float>(windowHeight), 0.001F, 1000.0F);
    proj[1][1] *= -1.0f;

    glm::mat4 mvp = proj * view * model;

    /* Render loop */
    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(pWindow))
    {
        // Convert units to seconds
        const float dt = cpuFrameTime * 0.001F;
        glfwPollEvents();

        // model = glm::rotate(model, 0.5F * dt, glm::vec3(1.0F, 1.0F, 1.0F));
        view = cam.Update(pWindow, dt, cameraSpeed, cameraSensitivity);
        proj = glm::perspectiveFov(
            glm::radians(45.0f), static_cast<float>(windowWidth), static_cast<float>(windowHeight), 0.001F, 1000.0F);
        proj[1][1] *= -1.0f;
        mvp = proj * view * model;

        /* Prepare the frame */

        const uint32_t frameIndex = pDevice->GetCurrentFrameInFlightIndex();

        pDevice->WaitForFence(frame[frameIndex].inFlightFence);
        const Grace::FrameSyncGroup& fsg = pDevice->AcquireNextSwapchainImage({ windowWidth, windowHeight });
        pDevice->ResetFence(frame[frameIndex].inFlightFence);

        // Reset command pool, which will reset all command buffers allocated from it too
        frame[frameIndex].pCmdPool->Reset();

        pDevice->UpdateBindlessDescriptorSet();

        /* Record commands */

        Grace::CommandBuffer& cmd = frame[frameIndex].cmd;
        cmd.BeginRecording();
        cmd.ResetQueryPoolFullRange<Grace::QueryType::Timestamp>(frameIndex);
        cmd.WriteTimestamp("GPU Frame Begin", VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frameIndex);

        const Grace::ImageHandle swapchainImg = pDevice->GetRecentlyAcquiredSwapchainImage();

        cmd.AddImageBarrier(swapchainImg, { Grace::AccessType::None }, { Grace::AccessType::BlitWrite });
        cmd.PipelineBarrier();

        cmd.BeginDebugLabel("Clear Resources");

        cmd.BindPipeline(mainPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.BindDescriptorSets(
            VK_PIPELINE_BIND_POINT_COMPUTE, pDevice->GetSolePipelineLayout(), 0, { pDevice->GetSoleDescriptorSet() });

        {
            struct PC
            {
                uint32_t renderImgId;
                uint32_t depthImgId;
            } pc;

            pc.renderImgId = pDevice->GetImage(renderImg).GetStorageImgId();
            pc.depthImgId = pDevice->GetImage(depthImg).GetStorageImgId();
            cmd.PushConstants(pDevice->GetSolePipelineLayout(), sizeof(pc), &pc);
        }

        cmd.InsertDebugLabel("Render/Depth Images");
        cmd.Dispatch(glm::ceil(windowWidth / 16.0F), glm::ceil(windowHeight / 16.0F));

        cmd.FillBuffer(metaDataBuffer, 0);
        cmd.FillBuffer(compactedInstrinsicDataBuffer, 0);
        cmd.FillBuffer(graphicsPipelineInstrinsicDataBuffer, 0);

        cmd.AddMemoryBarrier({ Grace::AccessType::ClearWrite },
                             { Grace::AccessType::ComputeShaderStorageRead, Grace::AccessType::ClearWrite });
        cmd.PipelineBarrier();

        cmd.EndDebugLabel();

        // Vertex Shader Stage
        cmd.BeginDebugLabel("Vertex Shader Stage");
        cmd.BindPipeline(vertexStagePipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

        {
            struct PC
            {
                glm::mat4 mvp;
                uint64_t intrinsicDataBuffer;
                uint64_t vertexBuffer;
                uint64_t indexBuffer;
                uint32_t indexCount;
            } pc;

            pc.mvp = mvp;
            pc.intrinsicDataBuffer = pDevice->GetBuffer(graphicsPipelineInstrinsicDataBuffer).GetBDA();
            pc.vertexBuffer = pDevice->GetBuffer(vertexBuffer).GetBDA();
            pc.indexBuffer = pDevice->GetBuffer(indexBuffer).GetBDA();
            pc.indexCount = testScene.value().indices.size();
            cmd.PushConstants(pDevice->GetSolePipelineLayout(), sizeof(pc), &pc);
        }

        cmd.Dispatch(glm::floor(testScene.value().indices.size() / 256.0F) + 1);
        cmd.EndDebugLabel();

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        struct PC
        {
            uint64_t tileDataBuffer;
            uint64_t intrinsicDataBuffer;
            uint64_t compactedIntrinsicDataBuffer;
            uint64_t metaDataBuffer;
            uint32_t triangleCount;
            uint32_t verticesCount;
            uint32_t renderImgId;
        } pc;

        pc.tileDataBuffer = pDevice->GetBuffer(tileDataBuffer).GetBDA();
        pc.intrinsicDataBuffer = pDevice->GetBuffer(graphicsPipelineInstrinsicDataBuffer).GetBDA();
        pc.compactedIntrinsicDataBuffer = pDevice->GetBuffer(compactedInstrinsicDataBuffer).GetBDA();
        pc.metaDataBuffer = pDevice->GetBuffer(metaDataBuffer).GetBDA();
        pc.triangleCount = testScene.value().indices.size() / 3;
        pc.verticesCount = testScene.value().indices.size();
        pc.renderImgId = pDevice->GetImage(renderImg).GetStorageImgId();
        cmd.PushConstants(pDevice->GetSolePipelineLayout(), sizeof(pc), &pc);

        cmd.BeginDebugLabel("Primitive Assembly Stage");
        cmd.BindPipeline(primitiveAsmPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.Dispatch(glm::floor(pc.triangleCount / 256.0F) + 1);
        cmd.EndDebugLabel();

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        // TODO: Make this an indirect dispatch (valid vertices / (1024 * 1024))
        cmd.BeginDebugLabel("Native Primitives Compaction Stage");
        cmd.BindPipeline(nativePrimitivesCompactionPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.Dispatch(1);
        cmd.EndDebugLabel();

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        cmd.BeginDebugLabel("Clipper Emitted Primitives Compaction Stage");
        cmd.BindPipeline(clipperEmittedPrimitivesCompactionPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.Dispatch(1);
        cmd.EndDebugLabel();

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        cmd.BeginDebugLabel("Binning Rasterizer Stage");
        cmd.InsertDebugLabel("Fill Primitive Bitmap");
        cmd.BindPipeline(ultraCoarseRasterizerPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.Dispatch(longLifetimeGlobalVars.ultraCoarseRasterDispatchSize.x,
                     longLifetimeGlobalVars.ultraCoarseRasterDispatchSize.y);

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        cmd.InsertDebugLabel("Fill Density Bitmap");
        cmd.BindPipeline(ultraCoarseRasterizerDensityBitmapPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.Dispatch(longLifetimeGlobalVars.ultraCoarseRasterDispatchSize.x
                     * longLifetimeGlobalVars.ultraCoarseRasterDispatchSize.y);

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        cmd.InsertDebugLabel("Fine Rasterizer");
        cmd.BindPipeline(fineRasterizerPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.Dispatch(longLifetimeGlobalVars.fineRasterDispatchSize.x, longLifetimeGlobalVars.fineRasterDispatchSize.y);

        cmd.EndDebugLabel();

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite }, { Grace::AccessType::BlitRead });
        cmd.PipelineBarrier();

        // Blit renderImg to swapchain and transition to present
        const VkImageBlit2 blit = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = {
                VkOffset3D(0, 0, 0),
                VkOffset3D(windowWidth, windowHeight, 1),
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .dstOffsets = {
                VkOffset3D(0, 0, 0),
                VkOffset3D(windowWidth, windowHeight, 1),
            },
        };

        cmd.BlitImage({
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcImage = pDevice->GetImage(renderImg).GetImage(),
            .srcImageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .dstImage = pDevice->GetImage(swapchainImg).GetImage(),
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit,
            .filter = VK_FILTER_LINEAR,
        });

        cmd.AddImageBarrier(pDevice->GetRecentlyAcquiredSwapchainImage(),
                            { Grace::AccessType::BlitWrite },
                            { Grace::AccessType::Present });
        cmd.PipelineBarrier();

        /* Wrap up the frame */

        cmd.WriteTimestamp("GPU Frame End", VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, frameIndex);

        // Finish recording for the command buffer for this frame
        cmd.EndRecording();

        pDevice->Submit(Grace::QueueFamily::Graphics, cmd, fsg, frame[frameIndex].inFlightFence);
        const Grace::SwapchainStatus ss = pDevice->Present(fsg);

        const Grace::TimestampQueryGroup& tqg =
            pDevice->GetQueryPoolResults<Grace::QueryType::Timestamp>(0, 0, VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

        tqg.DurationIfAvailable<Grace::TimestampUnits::Milliseconds>(
            gpuFrameTime, "GPU Frame Begin", "GPU Frame End", frameIndex);

        if (ss == Grace::SwapchainStatus::ShouldResize || framebufferHasResized)
        {
            framebufferHasResized = false;

            // Handle minimisation
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(pWindow, &width, &height);
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(pWindow, &width, &height);
                glfwWaitEvents();
            }
            windowWidth = width;
            windowHeight = height;

            pDevice->CreateSwapchain({ windowWidth, windowHeight }, vsync);

            pDevice->FreeImage(renderImg);
            renderImg = pDevice->CreateImage({
                .name = "GSR::renderImg",
                .dimensions = { .width = windowWidth, .height = windowHeight, .depth = 1 },
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                .access = Grace::AccessType::General,
                .size = 0,
                .data = nullptr,
                .mipmapped = false,
            });

            pDevice->FreeImage(depthImg);
            depthImg = pDevice->CreateImage({
                .name = "GSR::depthImg",
                .dimensions = { windowWidth, windowHeight, 1 },
                .format = VK_FORMAT_R32_SFLOAT,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT,
                .access = Grace::AccessType::General,
                .size = 0,
                .data = nullptr,
                .mipmapped = false,
            });

            longLifetimeGlobalVars.framebufferSize = { windowWidth, windowHeight };
            longLifetimeGlobalVars.ultraCoarseRasterDispatchSize = {
                glm::floor(static_cast<float>(windowWidth) / 256) + 1,
                glm::floor(static_cast<float>(windowHeight) / 256) + 1
            };
            longLifetimeGlobalVars.coarseRasterDispatchSize = { glm::floor(static_cast<float>(windowWidth) / 64) + 1,
                                                                glm::floor(static_cast<float>(windowHeight) / 64) + 1 };
            longLifetimeGlobalVars.fineRasterDispatchSize = { glm::floor(static_cast<float>(windowWidth) / 16) + 1,
                                                              glm::floor(static_cast<float>(windowHeight) / 16) + 1 };
            longLifetimeGlobalVars.clipperEmittedVerticesOffset = 12'500'000 - 4096,

            pDevice->FreeBuffer(longLifetimeGlobalVarsBuffer);
            longLifetimeGlobalVarsBuffer = pDevice->CreateBuffer({
                .name = "GSR::longLifetimeGlobalVarsBuffer",
                .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                .allocFlags = 0,
                .size = sizeof(LongLifetimeGlobalVars),
                .data = &longLifetimeGlobalVars,
            });
        }
        else if (ss == Grace::SwapchainStatus::Failure)
        {
            break;
        }

        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        cpuFrameTime = std::chrono::duration_cast<std::chrono::microseconds>(now - lastTime).count() / 1000.0F;
        lastTime = now;

        const std::string windowTitle =
            std::format("Grace-Software-Rasterizer | CPU Frame Time: {}ms | GPU Frame Time: {}ms",
                        cpuFrameTime,
                        static_cast<float>(gpuFrameTime));
        glfwSetWindowTitle(pWindow, windowTitle.c_str());

        pDevice->AdvanceToNextFrame();
    }

    glfwTerminate();
    return 0;
}
