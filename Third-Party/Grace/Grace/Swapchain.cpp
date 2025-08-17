#include "Swapchain.hpp"

#include <algorithm>
#include <string>
#include <cassert>
#include <limits>

#ifdef NO_GLFW
#include <glfw/glfw3.h>
#endif

#include <Grace/Context.hpp>
#include <Grace/DebugReporter.hpp>
#include <Grace/HelperFunctions.hpp>

namespace Grace
{

const VkSwapchainKHR& Swapchain::GetVkHandle() const
{
    return m_Swapchain;
}

SwapchainStatus Swapchain::GetStatus() const
{
    return m_SwapchainStatus;
}

const VkFormat& Swapchain::GetFormat() const
{
    // All images have the same format
    return m_Device->GetImage(m_Images[0]).GetFormat();
}

ImageHandle Swapchain::GetRecentAcquiredImage() const
{
    const FrameSyncGroup& sync = GetRecentFrameSyncGroup();
    assert(sync.imageIndex != ~0U);
    return m_Images[sync.imageIndex];
}

void Swapchain::Create(VkExtent2D imageExtent)
{
    VkPhysicalDevice physicalDevice = m_Device->GetPhysicalDevice();
    VkSurfaceKHR surfaceKHR = m_Device->GetSurface();

    const SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice, surfaceKHR);

    const VkSurfaceFormatKHR surfaceFormat = SelectSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = SelectSwapPresentMode(swapChainSupport.presentModes);

    if (!m_VSyncOn)
    {
        presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkExtent2D extent = SelectSwapExtent(imageExtent, swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // One more than image count to be safe
    m_ImageAcquiredSyncStructs.resize(imageCount + 1);

    // Make sure not to exceed max image count, 0 means no limit
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surfaceKHR;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = true;
    createInfo.oldSwapchain = m_Swapchain;

    std::array<uint32_t, 2> queueFamilyIndices = { m_Device->GetQueueFamilyIndex(QueueFamily::Graphics),
                                                   m_Device->GetQueueFamilyIndex(QueueFamily::Present) };

    if (m_Device->GetQueueFamilyIndex(QueueFamily::Graphics) != m_Device->GetQueueFamilyIndex(QueueFamily::Present))
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    VkSwapchainKHR tempSwapchain = {};
    DebugReporter::Check(vkCreateSwapchainKHR(m_Device->GetVkHandle(), &createInfo, nullptr, &tempSwapchain));
    assert(tempSwapchain != nullptr);

    if (m_Swapchain != nullptr)
    {
        Cleanup();
    }

    m_Swapchain = tempSwapchain;
    AssignDebugName<VkSwapchainKHR>(m_Device->GetVkHandle(), m_Swapchain, "Grace::SwapchainKHR");

    std::vector<VkImage> tempImages = {};
    vkGetSwapchainImagesKHR(m_Device->GetVkHandle(), m_Swapchain, &imageCount, nullptr);
    tempImages.resize(imageCount);
    m_Images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device->GetVkHandle(), m_Swapchain, &imageCount, tempImages.data());

    for (size_t i = 0; i < tempImages.size(); ++i)
    {
        const std::string name = "Grace::SwapchainImage::" + std::to_string(i);
        m_Images[i] = m_Device->CreateSwapchainImage(
            tempImages[i],
            {
                .name = name.c_str(),
                .dimensions = { extent.width, extent.height, 1 },
                .format = surfaceFormat.format,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .mipmapped = false,
            });
    }

    for (uint32_t i = 0; i < m_ImageAcquiredSyncStructs.size(); ++i)
    {
        FrameSyncGroup& sync = m_ImageAcquiredSyncStructs[i];

        const std::string acquireSemaphoreDebugName = "Grace::Semaphore::Acquire::" + std::to_string(i);
        sync.acquireSemaphore = m_Device->CreateBinarySemaphore({ .name = acquireSemaphoreDebugName.c_str() });

        const std::string presentSemaphoreDebugName = "Grace::Semaphore::Present::" + std::to_string(i);
        sync.presentSemaphore = m_Device->CreateBinarySemaphore({ .name = presentSemaphoreDebugName.c_str() });
    }
}

void Swapchain::Cleanup()
{
    // Destroys VkSwapchain and VkImages
    vkDestroySwapchainKHR(m_Device->GetVkHandle(), m_Swapchain, nullptr);

    for (ImageHandle& imageHandle : m_Images)
    {
        m_Device->FreeImage(imageHandle);
    }
}

Swapchain::~Swapchain()
{
    Cleanup();
}

Swapchain::Swapchain(Device* pDevice, VkExtent2D imageExtent, bool vsync) : m_Device(pDevice), m_VSyncOn(vsync)
{
    assert(!m_Device->IsNull());

    Create(imageExtent);
}

FrameSyncGroup& Swapchain::AcquireNextImage(VkExtent2D imageExtent)
{
    // Advance the cycle index
    m_ImageAcquiredCycleIndex = (m_ImageAcquiredCycleIndex + 1) % (m_ImageAcquiredSyncStructs.size() - 1);
    // Get FrameSyncGroup instance, that is not currently in use, from the cycle
    FrameSyncGroup& frameSync = m_ImageAcquiredSyncStructs[m_ImageAcquiredCycleIndex];
    const BinarySemaphore& acqSem = m_Device->GetBinarySemaphore(frameSync.acquireSemaphore);

    // Acquire an image from the swap chain
    VkResult result = vkAcquireNextImageKHR(m_Device->GetVkHandle(),
                                            m_Swapchain,
                                            UINT64_MAX,
                                            acqSem.GetVkSemaphore(),
                                            nullptr,
                                            &frameSync.imageIndex);

    // Check if swap chain is still adequate to present
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Create(imageExtent);
        m_SwapchainStatus = SwapchainStatus::ShouldResize;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        m_SwapchainStatus = SwapchainStatus::Failure;
        assert("Failed to acquire swap chain image!");
    }

    m_SwapchainStatus = SwapchainStatus::Success;
    return frameSync;
}

const FrameSyncGroup& Swapchain::GetRecentFrameSyncGroup() const
{
    return m_ImageAcquiredSyncStructs[m_ImageAcquiredCycleIndex];
}

VkSurfaceFormatKHR Swapchain::SelectSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    assert(availableFormats.size() > 0);

    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Swapchain::SelectSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        // Triple buffer mode
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::SelectSwapExtent(VkExtent2D imageExtent, const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    imageExtent.width =
        std::clamp(imageExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    imageExtent.height =
        std::clamp(imageExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return imageExtent;
}

} // namespace Grace
