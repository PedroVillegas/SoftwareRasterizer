#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <Grace/Image.hpp>
#include <Grace/Semaphore.hpp>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

class Device;

enum class SwapchainStatus : uint8_t
{
    /// Swapchain is in the optimal state to be presented
    Success,
    /// Swapchain is either Out Of Date or Suboptimal and should be resized/reconstructed
    ShouldResize,
    /// Swapchain's associated VkSurfaceKHR was lost and needs reconstruction, as does the VkSwapchainKHR
    Failure,
    /// Swapchain's status is not yet known
    Unknown
};

struct GRACE_EXPORT FrameSyncGroup
{
    BinarySemaphoreHandle acquireSemaphore = {};
    BinarySemaphoreHandle presentSemaphore = {};
    uint32_t imageIndex = ~0U;
};

class Swapchain
{
public:
    ~Swapchain();
    Swapchain() = default;
    Swapchain(Device* pDevice, VkExtent2D imageExtent, bool vsync);

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    Swapchain(Swapchain&& other) noexcept = delete;
    Swapchain& operator=(Swapchain&& other) noexcept = delete;

    FrameSyncGroup& AcquireNextImage(VkExtent2D imageExtent);

    GRACE_NODISCARD const FrameSyncGroup& GetRecentFrameSyncGroup() const;

    GRACE_NODISCARD const VkSwapchainKHR& GetVkHandle() const;

    GRACE_NODISCARD SwapchainStatus GetStatus() const;

    GRACE_NODISCARD const VkFormat& GetFormat() const;

    GRACE_NODISCARD ImageHandle GetRecentAcquiredImage() const;

private:
    void Create(VkExtent2D imageExtent);

    void Cleanup();

    GRACE_NODISCARD VkSurfaceFormatKHR SelectSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    GRACE_NODISCARD VkPresentModeKHR SelectSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    GRACE_NODISCARD VkExtent2D SelectSwapExtent(VkExtent2D imageExtent, const VkSurfaceCapabilitiesKHR& capabilities);

private:
    Device* m_Device = nullptr;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    std::vector<ImageHandle> m_Images = {};
    std::vector<FrameSyncGroup> m_ImageAcquiredSyncStructs = {};
    uint32_t m_ImageAcquiredCycleIndex = 0U;
    SwapchainStatus m_SwapchainStatus = SwapchainStatus::Unknown;
    bool m_VSyncOn = true;
};

} // namespace Grace
