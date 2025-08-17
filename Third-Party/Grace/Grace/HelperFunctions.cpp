#include "HelperFunctions.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <cmath>

#include <Grace/SyncGroup.hpp>
#include <Grace/DebugReporter.hpp>

namespace Grace
{

VkImageSubresourceRange EntireImageSubresourceRange(VkImageAspectFlags aspectMask)
{
    return {
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
}

VkImageAspectFlags DetermineImageAspectFlagsFromFormat(VkFormat format)
{
    // clang-format off
    switch (format)
    {
    case VK_FORMAT_D16_UNORM: GRACE_FALLTHROUGH;
    case VK_FORMAT_D32_SFLOAT:
        return VK_IMAGE_ASPECT_DEPTH_BIT;

    case VK_FORMAT_D16_UNORM_S8_UINT: GRACE_FALLTHROUGH;
    case VK_FORMAT_D24_UNORM_S8_UINT: GRACE_FALLTHROUGH;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    case VK_FORMAT_S8_UINT:
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
    // clang-format on
}

VkRenderingAttachmentInfo ColourAttachmentInfo(const Image& image, VkClearValue* clear, VkImageLayout imageLayout)
{
    assert(!image.IsNull());

    VkRenderingAttachmentInfo colourAttachment = {};
    colourAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colourAttachment.pNext = nullptr;

    colourAttachment.imageView = image.GetDefaultView().GetVkHandle();
    colourAttachment.imageLayout = imageLayout;
    colourAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    if (clear)
    {
        colourAttachment.clearValue = *clear;
    }

    return colourAttachment;
}

VkRenderingAttachmentInfo DepthAttachmentInfo(const Image& image, VkImageLayout imageLayout)
{
    assert(!image.IsNull());

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.pNext = nullptr;

    depthAttachment.imageView = image.GetDefaultView().GetVkHandle();
    depthAttachment.imageLayout = imageLayout;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil.depth = 0.0F;

    return depthAttachment;
}

VkRenderingInfo RenderingInfo(VkExtent2D renderArea,
                              uint32_t colourAttachmentCount,
                              const VkRenderingAttachmentInfo* pColourAttachments,
                              const VkRenderingAttachmentInfo* pDepthAttachment)
{
    // pColourAttachments and pDepthAttachment CAN be nullptrs
    return VkRenderingInfo(
        { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
          .pNext = nullptr,
          .renderArea = VkRect2D { { 0, 0 }, { (uint32_t) renderArea.width, (uint32_t) renderArea.height } },
          .layerCount = 1,
          .colorAttachmentCount = colourAttachmentCount,
          .pColorAttachments = pColourAttachments,
          .pDepthAttachment = pDepthAttachment });
}

void CopyImageToImage(CommandBuffer cmd,
                      const Image& src,
                      const Image& dst,
                      VkExtent2D srcExtent,
                      VkExtent2D dstExtent,
                      uint32_t srcMipLevel,
                      uint32_t dstMipLevel)
{
    assert(!cmd.IsNull());
    assert(!src.IsNull());
    assert(!dst.IsNull());

    VkImageBlit2 blitRegion = {};
    blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    blitRegion.pNext = nullptr;
    blitRegion.srcOffsets[1].x = srcExtent.width;
    blitRegion.srcOffsets[1].y = srcExtent.height;
    blitRegion.srcOffsets[1].z = 1;
    blitRegion.dstOffsets[1].x = dstExtent.width;
    blitRegion.dstOffsets[1].y = dstExtent.height;
    blitRegion.dstOffsets[1].z = 1;
    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcSubresource.mipLevel = srcMipLevel;
    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstSubresource.mipLevel = dstMipLevel;

    VkBlitImageInfo2 blitInfo = {};
    blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitInfo.pNext = nullptr;
    blitInfo.dstImage = dst.GetImage();
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    blitInfo.srcImage = src.GetImage();
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    cmd.BlitImage(blitInfo);
}

VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo* cmdInfo,
                         VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                         VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
    assert(cmdInfo != nullptr);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.pNext = nullptr;

    submitInfo.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreInfo;

    submitInfo.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreInfo;

    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = cmdInfo;

    return submitInfo;
}

VkPipelineShaderStageCreateInfo ShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule module)
{
    assert(module != nullptr);

    return VkPipelineShaderStageCreateInfo({ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                             .pNext = nullptr,
                                             .stage = stage,
                                             .module = module,
                                             .pName = "main" });
}

bool CreateShaderModule(VkDevice device, const std::filesystem::path& filename, VkShaderModule& shaderModule)
{
    assert(device != nullptr);
    assert(std::filesystem::exists(filename));

    auto shaderCode = ReadSpvFile(filename);

    VkShaderModuleCreateInfo smci = {};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = shaderCode.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    DebugReporter::Check(vkCreateShaderModule(device, &smci, nullptr, &shaderModule));
    const std::string shaderModuleDebugName = filename.filename().string();
    AssignDebugName<VkShaderModule>(device, shaderModule, shaderModuleDebugName.c_str());

    return true;
}

std::vector<char> ReadSpvFile(const std::filesystem::path& filename)
{
    assert(std::filesystem::exists(filename));

    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        std::cout << "Failed to open file!\n";
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surfaceKHR)
{
    assert(physicalDevice != nullptr);

    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surfaceKHR, &presentSupport);

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.IsComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surfaceKHR)
{
    assert(physicalDevice != nullptr);

    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surfaceKHR, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surfaceKHR, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surfaceKHR, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surfaceKHR, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surfaceKHR, &presentModeCount, details.presentModes.data());
    }

    return details;
}

} // namespace Grace
