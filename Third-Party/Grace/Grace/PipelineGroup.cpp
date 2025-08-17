#include "PipelineGroup.hpp"

#include <iostream>
#include <cassert>

#include <Grace/DebugReporter.hpp>
#include <Grace/Context.hpp>
#include <Grace/HelperFunctions.hpp>

namespace Grace
{

PipelineLayout::~PipelineLayout()
{
    if (m_PipelineLayout != nullptr)
    {
        vkDestroyPipelineLayout(m_Device->GetVkHandle(), m_PipelineLayout, nullptr);
    }
}

PipelineLayout::PipelineLayout(Device* pDevice, const PipelineLayoutDesc& desc) : m_Device(pDevice)
{
    assert(!m_Device->IsNull());

    VkPipelineLayoutCreateInfo plcInfo = {};
    plcInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plcInfo.pNext = nullptr;
    plcInfo.flags = desc.flags;
    plcInfo.setLayoutCount = static_cast<uint32_t>(desc.setLayouts.size());
    plcInfo.pSetLayouts = desc.setLayouts.data();
    plcInfo.pushConstantRangeCount = static_cast<uint32_t>(desc.pushConstantRanges.size());
    plcInfo.pPushConstantRanges = desc.pushConstantRanges.data();

    DebugReporter::Check(vkCreatePipelineLayout(m_Device->GetVkHandle(), &plcInfo, nullptr, &m_PipelineLayout));
    AssignDebugName(m_Device->GetVkHandle(), m_PipelineLayout, desc.name);
}

PipelineLayout::PipelineLayout(PipelineLayout&& other) noexcept
    : m_Device(other.m_Device), m_PipelineLayout(other.m_PipelineLayout)
{
    other.m_PipelineLayout = nullptr;
}

PipelineLayout& PipelineLayout::operator=(PipelineLayout&& other) noexcept
{
    if (m_PipelineLayout != nullptr)
    {
        vkDestroyPipelineLayout(m_Device->GetVkHandle(), m_PipelineLayout, nullptr);
    }

    m_Device = other.m_Device;
    m_PipelineLayout = other.m_PipelineLayout;
    other.m_PipelineLayout = nullptr;

    return *this;
}

bool PipelineLayout::IsNull() const
{
    return m_PipelineLayout == nullptr;
}

VkPipelineLayout PipelineLayout::GetVkPipelineLayout() const
{
    return m_PipelineLayout;
}

Pipeline::~Pipeline()
{
    if (m_Pipeline != nullptr)
    {
        vkDestroyPipeline(m_Device->GetVkHandle(), m_Pipeline, nullptr);
    }
}

Pipeline::Pipeline(Device* pDevice, const PipelineDesc& desc) : m_Device(pDevice)
{
    assert(!m_Device->IsNull());

    if (desc.type == PipelineType::Compute)
    {
        DebugReporter::Check(vkCreateComputePipelines(
            m_Device->GetVkHandle(), nullptr, 1, &desc.computeCreateInfo, nullptr, &m_Pipeline));
    }
    else if (desc.type == PipelineType::Graphics)
    {
        DebugReporter::Check(vkCreateGraphicsPipelines(
            m_Device->GetVkHandle(), nullptr, 1, &desc.graphicsCreateInfo, nullptr, &m_Pipeline));
    }

    if (m_Pipeline != nullptr)
    {
        AssignDebugName<VkPipeline>(m_Device->GetVkHandle(), m_Pipeline, desc.name);
    }
}

Pipeline::Pipeline(Pipeline&& other) noexcept : m_Device(other.m_Device), m_Pipeline(other.m_Pipeline)
{
    other.m_Pipeline = nullptr;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
{
    if (m_Pipeline != nullptr)
    {
        vkDestroyPipeline(m_Device->GetVkHandle(), m_Pipeline, nullptr);
    }

    m_Device = other.m_Device;
    m_Pipeline = other.m_Pipeline;
    other.m_Pipeline = nullptr;

    return *this;
}

bool Pipeline::IsNull() const
{
    return m_Pipeline == nullptr;
}

VkPipeline Pipeline::GetVkHandle() const
{
    return m_Pipeline;
}

// ----------------------------------------------------------------------------------
//                              PIPELINE BUILDER
// ----------------------------------------------------------------------------------

PipelineBuilder::PipelineBuilder(Device* pDevice) : m_Device(pDevice)
{
    assert(!m_Device->IsNull());

    ClearAll();
}

PipelineBuilder::~PipelineBuilder()
{
    ClearAll();
}

PipelineBuilder& PipelineBuilder::BuildComputePipeline(const char* name, PipelineLayoutHandle layout)
{
    const PipelineLayout& pl = m_Device->GetPipelineLayout(layout);
    assert(!pl.IsNull());
    assert(m_ShaderStages.size() == 1);
    assert(m_ShaderModules.size() == 1);

    VkComputePipelineCreateInfo computePipelineCreateInfo = {};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = pl.GetVkPipelineLayout();
    computePipelineCreateInfo.stage = m_ShaderStages[0];

    pipelineDesc.type = PipelineType::Compute;
    pipelineDesc.name = name;
    pipelineDesc.computeCreateInfo = computePipelineCreateInfo;
    pipelineDesc.graphicsCreateInfo = {};

    return *this;
}

PipelineBuilder& PipelineBuilder::BuildGraphicsPipeline(const char* name, PipelineLayoutHandle layout)
{
    const PipelineLayout& pl = m_Device->GetPipelineLayout(layout);
    assert(!pl.IsNull());
    assert(m_ShaderStages.size() > 1);
    assert(m_ShaderModules.size() > 1);

    m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    m_ViewportState.pNext = nullptr;
    m_ViewportState.viewportCount = 1;
    m_ViewportState.scissorCount = 1;

    m_ColourBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_ColourBlending.pNext = nullptr;
    m_ColourBlending.logicOpEnable = VK_FALSE;
    m_ColourBlending.logicOp = VK_LOGIC_OP_COPY;
    m_ColourBlending.attachmentCount = 1;
    m_ColourBlending.pAttachments = &m_ColourBlendAttachment;

    m_RenderInfo.colorAttachmentCount = 1;
    m_RenderInfo.pColorAttachmentFormats = &m_ColourAttachmentFormat;

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = &m_RenderInfo;
    graphicsPipelineCreateInfo.stageCount = (uint32_t) m_ShaderStages.size();
    graphicsPipelineCreateInfo.pStages = m_ShaderStages.data();
    graphicsPipelineCreateInfo.pVertexInputState = &m_VertexInputInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &m_InputAssembly;
    graphicsPipelineCreateInfo.pViewportState = &m_ViewportState;
    graphicsPipelineCreateInfo.pRasterizationState = &m_Rasterizer;
    graphicsPipelineCreateInfo.pMultisampleState = &m_Multisampling;
    graphicsPipelineCreateInfo.pColorBlendState = &m_ColourBlending;
    graphicsPipelineCreateInfo.pDepthStencilState = &m_DepthStencil;
    graphicsPipelineCreateInfo.layout = pl.GetVkPipelineLayout();

    m_DynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    m_DynamicInfo.pNext = nullptr;
    m_DynamicInfo.flags = 0;
    m_DynamicInfo.pDynamicStates = &m_DynamicState[0];
    m_DynamicInfo.dynamicStateCount = 2;

    graphicsPipelineCreateInfo.pDynamicState = &m_DynamicInfo;

    pipelineDesc.type = PipelineType::Graphics;
    pipelineDesc.name = name;
    pipelineDesc.computeCreateInfo = {};
    pipelineDesc.graphicsCreateInfo = graphicsPipelineCreateInfo;

    return *this;
}

PipelineBuilder& PipelineBuilder::ClearShaders()
{
    m_ShaderStages.clear();

    if (m_ShaderModules.size() == 0)
        return *this;

    for (auto& module : m_ShaderModules)
    {
        vkDestroyShaderModule(m_Device->GetVkHandle(), module, nullptr);
    }

    m_ShaderModules.clear();

    return *this;
}

PipelineBuilder& PipelineBuilder::ClearAll()
{

    m_VertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    m_InputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    m_Rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    m_ColourBlending = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    m_Multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    m_DepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    m_RenderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    m_ViewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    m_DynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    ClearShaders();

    return *this;
}

PipelineBuilder& PipelineBuilder::AddShader(const std::string& shader, VkShaderStageFlagBits stage)
{
    std::filesystem::path filepath = GRACE_SPIRV_DIR "/" + shader;

    assert(std::filesystem::exists(filepath));

    VkShaderModule shaderModule;
    if (!CreateShaderModule(m_Device->GetVkHandle(), filepath, shaderModule))
    {
        std::cout << "Failed to build shader module for " << shader << "\n";
    }

    m_ShaderModules.push_back(shaderModule);
    m_ShaderStages.push_back(ShaderStageCreateInfo(stage, shaderModule));

    return *this;
}

PipelineBuilder& PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology)
{
    // Primitive restart is used for triangle strips and line strips
    m_InputAssembly.topology = topology;
    m_InputAssembly.primitiveRestartEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
    // Polygon mode controls wireframe vs solid rendering and point rendering
    m_Rasterizer.polygonMode = mode;
    m_Rasterizer.lineWidth = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    m_Rasterizer.cullMode = cullMode;
    m_Rasterizer.frontFace = frontFace;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetMultisamplingNone()
{
    m_Multisampling.sampleShadingEnable = VK_FALSE;
    // Multisampling defaulted to no multisampling (1 sample per pixel)
    m_Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    m_Multisampling.minSampleShading = 1.0f;
    m_Multisampling.pSampleMask = nullptr;
    // No alpha to coverage either
    m_Multisampling.alphaToCoverageEnable = VK_FALSE;
    m_Multisampling.alphaToOneEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetMultisampling(VkSampleCountFlagBits sampleCount)
{
    m_Multisampling.sampleShadingEnable = VK_FALSE;
    // Multisampling defaulted to no multisampling (1 sample per pixel)
    m_Multisampling.rasterizationSamples = sampleCount;
    m_Multisampling.minSampleShading = 1.0f;
    m_Multisampling.pSampleMask = nullptr;
    // No alpha to coverage either
    m_Multisampling.alphaToCoverageEnable = VK_FALSE;
    m_Multisampling.alphaToOneEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetColourAttachmentFormat(const VkFormat* pFormat)
{
    assert(pFormat != nullptr);

    m_ColourAttachmentFormat = *pFormat;
    // Connect the format to the renderInfo structure
    m_RenderInfo.colorAttachmentCount = 1;
    m_RenderInfo.pColorAttachmentFormats = pFormat;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthFormat(VkFormat format)
{
    m_RenderInfo.depthAttachmentFormat = format;

    return *this;
}

PipelineBuilder& PipelineBuilder::DisableBlending()
{
    // Default write mask
    m_ColourBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    m_ColourBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    m_ColourBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    m_ColourBlendAttachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
    // No blending
    m_ColourBlendAttachment.blendEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::DisableDepthTest()
{
    m_DepthStencil.depthTestEnable = VK_FALSE;
    m_DepthStencil.depthWriteEnable = VK_FALSE;
    m_DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
    m_DepthStencil.stencilTestEnable = VK_FALSE;
    m_DepthStencil.front = {};
    m_DepthStencil.back = {};
    m_DepthStencil.minDepthBounds = 0.f;
    m_DepthStencil.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::EnableDepthTest(bool depthWriteEnable, VkCompareOp op)
{
    m_DepthStencil.depthTestEnable = VK_TRUE;
    m_DepthStencil.depthWriteEnable = depthWriteEnable;
    m_DepthStencil.depthCompareOp = op;
    m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
    m_DepthStencil.stencilTestEnable = VK_FALSE;
    m_DepthStencil.front = {};
    m_DepthStencil.back = {};
    m_DepthStencil.minDepthBounds = 0.f;
    m_DepthStencil.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::EnableBlendingAdditive()
{
    m_ColourBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_ColourBlendAttachment.blendEnable = VK_TRUE;
    m_ColourBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    m_ColourBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    m_ColourBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    m_ColourBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    m_ColourBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    m_ColourBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    return *this;
}

PipelineBuilder& PipelineBuilder::EnableBlendingAlphaBlend()
{
    m_ColourBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    m_ColourBlendAttachment.blendEnable = VK_TRUE;
    m_ColourBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    m_ColourBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    m_ColourBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    m_ColourBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    m_ColourBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    m_ColourBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    return *this;
}

} // namespace Grace
