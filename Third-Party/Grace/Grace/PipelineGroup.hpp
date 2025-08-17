#pragma once

#include <array>
#include <vector>
#include <string>
#include <filesystem>

#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>
#include <Grace/HandleTypes.hpp>

namespace Grace
{

class Device;

enum class PipelineType : uint8_t
{
    Compute,
    Graphics
};

struct GRACE_EXPORT PipelineLayoutDesc
{
    const char* name;
    VkPipelineLayoutCreateFlags flags;
    std::vector<VkDescriptorSetLayout> setLayouts;
    std::vector<VkPushConstantRange> pushConstantRanges;
};

class GRACE_EXPORT PipelineLayout
{
public:
    ~PipelineLayout();
    PipelineLayout() = default;
    PipelineLayout(Device* pDevice, const PipelineLayoutDesc& desc);

    // Copy constructions/assignments are prohibited to stop destructor trying to
    // destroy the same VkPipelineLayout handle more than once
    PipelineLayout(const PipelineLayout&) = delete;
    PipelineLayout& operator=(const PipelineLayout&) = delete;

    PipelineLayout(PipelineLayout&& other) noexcept;
    PipelineLayout& operator=(PipelineLayout&& other) noexcept;

    GRACE_NODISCARD bool IsNull() const;

    GRACE_NODISCARD VkPipelineLayout GetVkPipelineLayout() const;

private:
    Device* m_Device = nullptr;
    VkPipelineLayout m_PipelineLayout = nullptr;
};

/// Description used to create a Pipeline object
struct GRACE_EXPORT PipelineDesc
{
    /// Name used to identify the pipeline, e.g. in validation errors
    const char* name = {};
    /// Specifies type of pipeline, compute or graphics
    PipelineType type = {};
    /// Structure required to create a compute pipeline
    VkComputePipelineCreateInfo computeCreateInfo = {};
    /// Structure required to create a graphics pipeline
    VkGraphicsPipelineCreateInfo graphicsCreateInfo = {};
};

class GRACE_EXPORT Pipeline
{
public:
    ~Pipeline();
    Pipeline() = default;
    Pipeline(Device* pDevice, const PipelineDesc& desc);

    // Copy constructions/assignments are prohibited to stop destructor trying to
    // destroy the same VkPipeline handle more than once
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;

    GRACE_NODISCARD bool IsNull() const;

    GRACE_NODISCARD VkPipeline GetVkHandle() const;

private:
    Device* m_Device = nullptr;
    VkPipeline m_Pipeline = nullptr;
};

class GRACE_EXPORT PipelineBuilder
{
public:
    ~PipelineBuilder();
    PipelineBuilder() = default;
    explicit PipelineBuilder(Device* pDevice);

    PipelineBuilder(const PipelineBuilder&) = delete;
    PipelineBuilder& operator=(const PipelineBuilder&) = delete;

    PipelineBuilder(PipelineBuilder&&) noexcept = delete;
    PipelineBuilder& operator=(PipelineBuilder&&) noexcept = delete;

    PipelineBuilder& BuildComputePipeline(const char* name, PipelineLayoutHandle layout);
    PipelineBuilder& BuildGraphicsPipeline(const char* name, PipelineLayoutHandle layout);

    PipelineBuilder& ClearAll();
    PipelineBuilder& ClearShaders();

    PipelineBuilder& AddShader(const std::string& shader, VkShaderStageFlagBits stage);
    PipelineBuilder& SetInputTopology(VkPrimitiveTopology topology);
    PipelineBuilder& SetPolygonMode(VkPolygonMode mode);
    PipelineBuilder& SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    PipelineBuilder& SetMultisamplingNone();
    PipelineBuilder& SetMultisampling(VkSampleCountFlagBits sampleCount);
    PipelineBuilder& SetColourAttachmentFormat(const VkFormat* pFormat);
    PipelineBuilder& SetDepthFormat(VkFormat format);
    PipelineBuilder& DisableBlending();
    PipelineBuilder& DisableDepthTest();
    PipelineBuilder& EnableDepthTest(bool depthWriteEnable, VkCompareOp op);
    PipelineBuilder& EnableBlendingAdditive();
    PipelineBuilder& EnableBlendingAlphaBlend();

public:
    PipelineDesc pipelineDesc = {};

private:
    Device* m_Device = nullptr;

    std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages = {};
    std::vector<VkShaderModule> m_ShaderModules = {};

    VkPipelineVertexInputStateCreateInfo m_VertexInputInfo = {};
    VkPipelineInputAssemblyStateCreateInfo m_InputAssembly = {};
    VkPipelineRasterizationStateCreateInfo m_Rasterizer = {};
    VkPipelineColorBlendAttachmentState m_ColourBlendAttachment = {};
    VkPipelineColorBlendStateCreateInfo m_ColourBlending = {};
    VkPipelineMultisampleStateCreateInfo m_Multisampling = {};
    VkPipelineDepthStencilStateCreateInfo m_DepthStencil = {};
    VkPipelineRenderingCreateInfo m_RenderInfo = {};
    VkPipelineViewportStateCreateInfo m_ViewportState = {};
    VkPipelineDynamicStateCreateInfo m_DynamicInfo = {};
    std::array<VkDynamicState, 2> m_DynamicState = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkFormat m_ColourAttachmentFormat = {};
};

} // namespace Grace
