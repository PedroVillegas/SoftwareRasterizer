#pragma once

#include <vector>
#include <filesystem>

#include <vulkan/vulkan.h>
#include <Grace/CommandGroup.hpp>
#include <Grace/Image.hpp>
#include <Grace/OptionalPFN.hpp>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

GRACE_NODISCARD GRACE_EXPORT VkImageSubresourceRange EntireImageSubresourceRange(VkImageAspectFlags aspectMask);

GRACE_NODISCARD GRACE_EXPORT VkImageAspectFlags DetermineImageAspectFlagsFromFormat(VkFormat format);

template <typename VK_HANDLE>
void AssignDebugName(VkDevice device, VK_HANDLE handle, const char* name)
{
    VkObjectType objectType = VK_OBJECT_TYPE_UNKNOWN;
    if constexpr (std::is_same_v<VK_HANDLE, VkInstance>)
    {
        objectType = VK_OBJECT_TYPE_INSTANCE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkPhysicalDevice>)
    {
        objectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDevice>)
    {
        objectType = VK_OBJECT_TYPE_DEVICE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkQueue>)
    {
        objectType = VK_OBJECT_TYPE_QUEUE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkSemaphore>)
    {
        objectType = VK_OBJECT_TYPE_SEMAPHORE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkSemaphore>)
    {
        objectType = VK_OBJECT_TYPE_SEMAPHORE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkCommandBuffer>)
    {
        objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkFence>)
    {
        objectType = VK_OBJECT_TYPE_FENCE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDeviceMemory>)
    {
        objectType = VK_OBJECT_TYPE_DEVICE_MEMORY;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkBuffer>)
    {
        objectType = VK_OBJECT_TYPE_BUFFER;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkImage>)
    {
        objectType = VK_OBJECT_TYPE_IMAGE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkEvent>)
    {
        objectType = VK_OBJECT_TYPE_EVENT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkQueryPool>)
    {
        objectType = VK_OBJECT_TYPE_QUERY_POOL;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkBufferView>)
    {
        objectType = VK_OBJECT_TYPE_BUFFER_VIEW;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkImageView>)
    {
        objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkShaderModule>)
    {
        objectType = VK_OBJECT_TYPE_SHADER_MODULE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkPipelineCache>)
    {
        objectType = VK_OBJECT_TYPE_PIPELINE_CACHE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkPipelineLayout>)
    {
        objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkRenderPass>)
    {
        objectType = VK_OBJECT_TYPE_RENDER_PASS;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkPipeline>)
    {
        objectType = VK_OBJECT_TYPE_PIPELINE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDescriptorSetLayout>)
    {
        objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkSampler>)
    {
        objectType = VK_OBJECT_TYPE_SAMPLER;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDescriptorPool>)
    {
        objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDescriptorSet>)
    {
        objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkFramebuffer>)
    {
        objectType = VK_OBJECT_TYPE_FRAMEBUFFER;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkCommandPool>)
    {
        objectType = VK_OBJECT_TYPE_COMMAND_POOL;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkSamplerYcbcrConversion>)
    {
        objectType = VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDescriptorUpdateTemplate>)
    {
        objectType = VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkPrivateDataSlot>)
    {
        objectType = VK_OBJECT_TYPE_PRIVATE_DATA_SLOT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkSurfaceKHR>)
    {
        objectType = VK_OBJECT_TYPE_SURFACE_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkSwapchainKHR>)
    {
        objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDisplayKHR>)
    {
        objectType = VK_OBJECT_TYPE_DISPLAY_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDisplayModeKHR>)
    {
        objectType = VK_OBJECT_TYPE_DISPLAY_MODE_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDebugReportCallbackEXT>)
    {
        objectType = VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkVideoSessionKHR>)
    {
        objectType = VK_OBJECT_TYPE_VIDEO_SESSION_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkVideoSessionParametersKHR>)
    {
        objectType = VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkCuModuleNVX>)
    {
        objectType = VK_OBJECT_TYPE_CU_MODULE_NVX;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkCuFunctionNVX>)
    {
        objectType = VK_OBJECT_TYPE_CU_FUNCTION_NVX;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDebugUtilsMessengerEXT>)
    {
        objectType = VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkAccelerationStructureKHR>)
    {
        objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkValidationCacheEXT>)
    {
        objectType = VK_OBJECT_TYPE_VALIDATION_CACHE_EXT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkAccelerationStructureNV>)
    {
        objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkPerformanceConfigurationINTEL>)
    {
        objectType = VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkDeferredOperationKHR>)
    {
        objectType = VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkIndirectCommandsLayoutEXT>)
    {
        objectType = VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkMicromapEXT>)
    {
        objectType = VK_OBJECT_TYPE_MICROMAP_EXT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkOpticalFlowSessionNV>)
    {
        objectType = VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkShaderEXT>)
    {
        objectType = VK_OBJECT_TYPE_SHADER_EXT;
    }
    else if constexpr (std::is_same_v<VK_HANDLE, VkPipelineBinaryKHR>)
    {
        objectType = VK_OBJECT_TYPE_PIPELINE_BINARY_KHR;
    }
#if (GRACE_TARGET_VULKAN_API_VERSION >= 14)
    else if constexpr (std::is_same_v<VK_HANDLE, VkExternalComputeQueueNV>)
    {
        objectType = VK_OBJECT_TYPE_EXTERNAL_COMPUTE_QUEUE_NV;
    }
#endif
    else if constexpr (std::is_same_v<VK_HANDLE, VkIndirectExecutionSetEXT>)
    {
        objectType = VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT;
    }

    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = uint64_t(handle);
    nameInfo.pObjectName = name;
    GRACE_SET_VK_DEBUG_NAME(device, &nameInfo);
}

GRACE_NODISCARD GRACE_EXPORT VkRenderingAttachmentInfo ColourAttachmentInfo(
    const Image& image, VkClearValue* clear, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

GRACE_NODISCARD GRACE_EXPORT VkRenderingAttachmentInfo
DepthAttachmentInfo(const Image& image, VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

GRACE_NODISCARD GRACE_EXPORT VkRenderingInfo RenderingInfo(VkExtent2D renderArea,
                                                           uint32_t colourAttachmentCount,
                                                           const VkRenderingAttachmentInfo* pColourAttachments,
                                                           const VkRenderingAttachmentInfo* pDepthAttachment);

void CopyImageToImage(CommandBuffer cmd,
                      const Image& src,
                      const Image& dst,
                      VkExtent2D srcExtent,
                      VkExtent2D dstExtent,
                      uint32_t srcMipLevel = 0U,
                      uint32_t dstMipLevel = 0U);

GRACE_NODISCARD VkSubmitInfo2 SubmitInfo(VkCommandBufferSubmitInfo* cmdInfo,
                                         VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                                         VkSemaphoreSubmitInfo* waitSemaphoreInfo);

GRACE_NODISCARD std::vector<char> ReadSpvFile(const std::filesystem::path& filename);

GRACE_NODISCARD bool
CreateShaderModule(VkDevice device, const std::filesystem::path& filename, VkShaderModule& shaderModule);

GRACE_NODISCARD VkPipelineShaderStageCreateInfo ShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                                                      VkShaderModule module);

struct QueueFamilyIndices
{
    std::optional<uint32_t> transferFamily = {};
    std::optional<uint32_t> computeFamily = {};
    std::optional<uint32_t> graphicsFamily = {};
    std::optional<uint32_t> presentFamily = {};

    bool IsComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

GRACE_NODISCARD QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surfaceKHR);

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats = {};
    std::vector<VkPresentModeKHR> presentModes = {};
};

GRACE_NODISCARD SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surfaceKHR);

} // namespace Grace
