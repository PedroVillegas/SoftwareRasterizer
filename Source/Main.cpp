#include <chrono>
#include <format>

#include <GLFW/glfw3.h>
#include <Grace/Grace.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

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

struct Camera
{
    glm::vec3 position = { 0.0F, 0.0F, 0.0F };
    glm::vec3 rotation = { 0.0F, 0.0F, 0.0F };
};

struct Vertex
{
    glm::vec3 position = { 0.0F, 0.0F, 0.0F };
    glm::vec2 uv = { 0.0F, 0.0F };
};

static glm::mat4 HandleCamera(GLFWwindow* pWindow, Camera& camera, float dt, float speed, float sens);

static void PrepareGraphicsPipeline(uint32_t vertexCount,
                                    uint32_t& previousVertexCount,
                                    Grace::BufferHandle& intrinsicVarsBuf,
                                    Grace::Device* pDevice);

int main()
{
    constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    // Timings
    double gpuFrameTime = 0.0;
    float cpuFrameTime = 0.0F;

    // Options
    const float cameraSpeed = 10.0F;
    const float cameraSensitivity = 90.0F;
    bool vsync = false;
    bool framebufferHasResized = false;
    uint32_t windowWidth = 800;
    uint32_t windowHeight = 600;

    uint32_t previousVertexCount = 0;

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

    const Grace::SamplerHandle linearWrapSampler = pDevice->CreateSampler({
        .minFilter = VK_FILTER_LINEAR,
        .magFilter = VK_FILTER_LINEAR,
        .addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    });

    Grace::BufferHandle graphicsPipelineInstrinsicVariablesBuffer = pDevice->CreateBuffer({
        .name = "GSR::graphicsPipelineInstrinsicVariablesBuffer",
        .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .allocFlags = 0,
        .size = 1,
        .data = nullptr,
    });

    Grace::BufferHandle fragShaderIndirectDispatchBuffer = pDevice->CreateBuffer({
        .name = "GSR::fragShaderIndirectDispatchBuffer",
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
               | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .allocFlags = 0,
        .size = (4 * sizeof(uint32_t)) + (windowWidth * windowHeight * 2 * sizeof(uint32_t)),
        .data = nullptr,
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
    pbuilder.AddShader("Rasterizer.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::rasterizerPipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle rasterizerPipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("FragmentDispatch.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::fragmentDispatchPipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle fragmentDispatchPipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    pbuilder.ClearShaders();
    pbuilder.AddShader("FragmentStage.slang.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pbuilder.BuildComputePipeline("GSR::fragmentStagePipeline", pDevice->GetSolePipelineLayout());
    const Grace::PipelineHandle fragmentStagePipeline = pDevice->CreatePipeline(pbuilder.pipelineDesc);

    // Cube vertex and index buffer
    // clang-format off
    const std::array<Vertex, 36> vertices = {
        Vertex(glm::vec3(-0.5F, -0.5F, -0.5F),  glm::vec2(0.0F, 0.0F)),
        Vertex(glm::vec3( 0.5F, -0.5F, -0.5F),  glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3( 0.5F,  0.5F, -0.5F),  glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3( 0.5F,  0.5F, -0.5F),  glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3(-0.5F,  0.5F, -0.5F),  glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3(-0.5F, -0.5F, -0.5F),  glm::vec2(0.0F, 0.0F)),

        Vertex(glm::vec3(-0.5F, -0.5F,  0.5F),  glm::vec2(0.0F, 0.0F)),
        Vertex(glm::vec3( 0.5F, -0.5F,  0.5F),  glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3( 0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3( 0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3(-0.5F,  0.5F,  0.5F),  glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3(-0.5F, -0.5F,  0.5F),  glm::vec2(0.0F, 0.0F)),

        Vertex(glm::vec3(-0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3(-0.5F,  0.5F, -0.5F),  glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3(-0.5F, -0.5F, -0.5F),  glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3(-0.5F, -0.5F, -0.5F),  glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3(-0.5F, -0.5F,  0.5F),  glm::vec2(0.0F, 0.0F)),
        Vertex(glm::vec3(-0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 0.0F)),

        Vertex(glm::vec3(0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3(0.5F,  0.5F, -0.5F),  glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3(0.5F, -0.5F, -0.5F),  glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3(0.5F, -0.5F, -0.5F),  glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3(0.5F, -0.5F,  0.5F),  glm::vec2(0.0F, 0.0F)),
        Vertex(glm::vec3(0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 0.0F)),

        Vertex(glm::vec3(-0.5F, -0.5F, -0.5F), glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3( 0.5F, -0.5F, -0.5F), glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3( 0.5F, -0.5F,  0.5F), glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3( 0.5F, -0.5F,  0.5F), glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3(-0.5F, -0.5F,  0.5F), glm::vec2(0.0F, 0.0F)),
        Vertex(glm::vec3(-0.5F, -0.5F, -0.5F), glm::vec2(0.0F, 1.0F)),

        Vertex(glm::vec3(-0.5F,  0.5F, -0.5F),  glm::vec2(0.0F, 1.0F)),
        Vertex(glm::vec3( 0.5F,  0.5F, -0.5F),  glm::vec2(1.0F, 1.0F)),
        Vertex(glm::vec3( 0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3( 0.5F,  0.5F,  0.5F),  glm::vec2(1.0F, 0.0F)),
        Vertex(glm::vec3(-0.5F,  0.5F,  0.5F),  glm::vec2(0.0F, 0.0F)),
        Vertex(glm::vec3(-0.5F,  0.5F, -0.5F),  glm::vec2(0.0F, 1.0F))
    };
    // clang-format on

    const Grace::BufferHandle vertexBuffer = pDevice->CreateBuffer({
        .name = "GSR::vertexBuffer",
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
               | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .size = vertices.size() * sizeof(Vertex),
        .data = vertices.data(),
    });

    // Load image from file using stbi
    int x, y, channels;
    uint8_t* data = stbi_load(RESOURCES_PATH "Images/UVCheckerMap10-1024.png", &x, &y, &channels, 4);
    const uint32_t textureSizeBytes = x * y * 4 * sizeof(uint8_t);

    // Create an image with image file metadata
    const Grace::ImageHandle texture = pDevice->CreateImage({
        .name = "GSR::texture",
        .dimensions = { static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1 },
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .size = textureSizeBytes,
        .data = data,
        .mipmapped = true,
    });
    stbi_image_free(data);

    // MVP
    Camera cam = {};
    cam.position.z = 3.0F;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -6.0f));
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
        view = HandleCamera(pWindow, cam, dt, cameraSpeed, cameraSensitivity);
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

        cmd.BeginDebugLabel("Render/Depth Image Clear");

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

        // Clear
        cmd.Dispatch(glm::floor(windowWidth / 16.0F) + 1, glm::floor(windowHeight / 16.0F) + 1);
        cmd.EndDebugLabel();

        // Vertex Shader Stage
        cmd.BeginDebugLabel("Vertex Shader Stage");
        cmd.BindPipeline(vertexStagePipeline, VK_PIPELINE_BIND_POINT_COMPUTE);

        PrepareGraphicsPipeline(
            vertices.size(), previousVertexCount, graphicsPipelineInstrinsicVariablesBuffer, pDevice);

        {
            struct PC
            {
                uint64_t intrinsicDataBuffer;
                uint64_t vbuffer;
                glm::mat4 mvp;
                uint32_t vertexCount;
            } pc;

            pc.intrinsicDataBuffer = pDevice->GetBuffer(graphicsPipelineInstrinsicVariablesBuffer).GetBDA();
            pc.vbuffer = pDevice->GetBuffer(vertexBuffer).GetBDA();
            pc.mvp = mvp;
            pc.vertexCount = vertices.size();
            cmd.PushConstants(pDevice->GetSolePipelineLayout(), sizeof(pc), &pc);
        }

        cmd.Dispatch(glm::floor(vertices.size() / 16.0F) + 1);
        cmd.EndDebugLabel();

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        cmd.BeginDebugLabel("Rasterizer Stage");
        cmd.BindPipeline(rasterizerPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        {
            struct PC
            {
                uint64_t intrinsicDataBuffer;
                uint64_t fragShaderIndirectDispatchBuffer;
                uint32_t triangleCount;
                uint32_t framebufferWidth;
                uint32_t framebufferHeight;
                uint32_t depthImgId;
            } pc;

            pc.intrinsicDataBuffer = pDevice->GetBuffer(graphicsPipelineInstrinsicVariablesBuffer).GetBDA();
            pc.fragShaderIndirectDispatchBuffer = pDevice->GetBuffer(fragShaderIndirectDispatchBuffer).GetBDA();
            pc.triangleCount = vertices.size() / 3;
            pc.framebufferWidth = windowWidth;
            pc.framebufferHeight = windowHeight;
            pc.depthImgId = pDevice->GetImage(depthImg).GetStorageImgId();
            cmd.PushConstants(pDevice->GetSolePipelineLayout(), sizeof(pc), &pc);
        }
        cmd.Dispatch(glm::floor((vertices.size() / 3) / 16.0F) + 1);

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        cmd.BindPipeline(fragmentDispatchPipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        cmd.Dispatch(1);

        cmd.EndDebugLabel();

        cmd.AddMemoryBarrier({ Grace::AccessType::ComputeShaderWrite },
                             { Grace::AccessType::ComputeShaderStorageRead });
        cmd.PipelineBarrier();

        cmd.BeginDebugLabel("Fragment Shader Stage");

        cmd.BindPipeline(fragmentStagePipeline, VK_PIPELINE_BIND_POINT_COMPUTE);
        {
            struct PC
            {
                uint64_t fragShaderIndirectDispatchBuffer;
                uint32_t renderImgId;
            } pc;

            pc.renderImgId = pDevice->GetImage(renderImg).GetStorageImgId();
            pc.fragShaderIndirectDispatchBuffer = pDevice->GetBuffer(fragShaderIndirectDispatchBuffer).GetBDA();
            cmd.PushConstants(pDevice->GetSolePipelineLayout(), sizeof(pc), &pc);
        }
        cmd.DispatchIndirect(fragShaderIndirectDispatchBuffer, 0);

        cmd.EndDebugLabel();

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

            pDevice->FreeBuffer(fragShaderIndirectDispatchBuffer);
            fragShaderIndirectDispatchBuffer = pDevice->CreateBuffer({
                .name = "GSR::fragShaderIndirectDispatchBuffer",
                .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                       | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                .allocFlags = 0,
                .size = (4 * sizeof(uint32_t)) + (windowWidth * windowHeight * 2 * sizeof(uint32_t)),
                .data = nullptr,
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

void PrepareGraphicsPipeline(uint32_t vertexCount,
                             uint32_t& previousVertexCount,
                             Grace::BufferHandle& intrinsicVarsBuf,
                             Grace::Device* pDevice)
{
    if (vertexCount > previousVertexCount)
    {
        if (!pDevice->GetBuffer(intrinsicVarsBuf).IsNull())
        {
            pDevice->FreeBuffer(intrinsicVarsBuf, true);
        }

        intrinsicVarsBuf = pDevice->CreateBuffer({
            .name = "GSR::graphicsPipelineInstrinsicVariablesBuffer",
            .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocFlags = 0,
            .size = vertexCount * sizeof(GraphicsPipelineIntrinsicData),
            .data = nullptr,
        });

        previousVertexCount = vertexCount;
    }
}

glm::mat4 HandleCamera(GLFWwindow* pWindow, Camera& camera, float dt, float speed, float sens)
{
    glm::vec3 rot = glm::vec3(0.0F);
    if (glfwGetKey(pWindow, GLFW_KEY_UP) == GLFW_PRESS)
    {
        rot.x += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        rot.x -= 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        rot.y += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        rot.y -= 1.0F;
    }
    if (glm::any(glm::notEqual(rot, glm::vec3(0.0F))))
    {
        camera.rotation += glm::normalize(rot) * sens * dt;
    }

    camera.rotation.y = glm::mod(camera.rotation.y, 360.0F);
    camera.rotation.x = glm::clamp(camera.rotation.x, -89.0F, 89.0F);

    glm::quat pitchRotation = glm::angleAxis(glm::radians(camera.rotation.x), glm::vec3 { 1.0F, 0.0F, 0.0F });
    glm::quat yawRotation = glm::angleAxis(glm::radians(camera.rotation.y), glm::vec3 { 0.0F, 1.0F, 0.0F });

    glm::mat4 R = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);

    glm::vec3 move = glm::vec3(0.0F);
    if (glfwGetKey(pWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        move.z -= 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        move.z += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        move.x -= 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        move.x += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        move.y += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        move.y -= 1.0F;
    }
    if (glm::any(glm::notEqual(move, glm::vec3(0.0F))))
    {
        camera.position += glm::normalize(glm::mat3(R) * move) * speed * dt;
    }
    glm::mat4 T = glm::translate(glm::mat4(1.0F), camera.position);

    return glm::inverse(T * R);
}
