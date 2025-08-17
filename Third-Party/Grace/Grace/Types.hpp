#pragma once
#include <vector>

namespace Grace
{

struct Rect2D
{
    VkExtent2D extent;
    VkOffset2D offset = { 0, 0 };
};

struct DynamicRenderingDesc
{
    Rect2D renderArea = {};
    std::vector<VkRenderingAttachmentInfo> colorAttachments = {};
    std::vector<VkRenderingAttachmentInfo> depthAttachments = {};
    std::vector<VkRenderingAttachmentInfo> stencilAttachments = {};
    VkRenderingFlags flags = 0;
    uint32_t layerCount = 1U;
    uint32_t viewMask = 0U;
};

} // namespace Grace
