#include "SyncGroup.hpp"

#include <cassert>

#include <Grace/DebugReporter.hpp>
#include <Grace/Buffer.hpp>
#include <Grace/Image.hpp>
#include <Grace/CommandGroup.hpp>
#include <Grace/HelperFunctions.hpp>

namespace Grace
{

BarrierBuilder& BarrierBuilder::PipelineBarrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier2 vkMemoryBarrier = {};

    uint32_t memoryBarrierCount =
        (m_MemoryBarrier.accessesBefore.empty() || m_MemoryBarrier.accessesAfter.empty()) ? 0 : 1;

    uint32_t bufferMemoryBarrierCount = static_cast<uint32_t>(m_BufferBarriers.size());
    std::vector<VkBufferMemoryBarrier2> vkBufferMemoryBarriers(bufferMemoryBarrierCount);

    uint32_t imageMemoryBarrierCount = static_cast<uint32_t>(m_ImageBarriers.size());
    std::vector<VkImageMemoryBarrier2> vkImageMemoryBarriers(imageMemoryBarrierCount);

    // Global memory barrier
    if (memoryBarrierCount > 0)
    {
        GetVulkanMemoryBarrier(m_MemoryBarrier, vkMemoryBarrier);
    }

    // Buffer memory barriers
    if (bufferMemoryBarrierCount > 0)
    {
        for (uint32_t i = 0; i < bufferMemoryBarrierCount; ++i)
        {
            GetVulkanBufferMemoryBarrier(m_BufferBarriers[i], vkBufferMemoryBarriers[i]);
        }
    }

    // Image memory barriers
    if (imageMemoryBarrierCount > 0)
    {
        for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i)
        {
            GetVulkanImageMemoryBarrier(m_ImageBarriers[i], vkImageMemoryBarriers[i]);
        }
    }

    VkDependencyInfo depInfo = {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;
    depInfo.dependencyFlags = 0;
    depInfo.memoryBarrierCount = memoryBarrierCount;
    depInfo.pMemoryBarriers = &vkMemoryBarrier;
    depInfo.imageMemoryBarrierCount = imageMemoryBarrierCount;
    depInfo.pImageMemoryBarriers = vkImageMemoryBarriers.data();
    depInfo.bufferMemoryBarrierCount = bufferMemoryBarrierCount;
    depInfo.pBufferMemoryBarriers = vkBufferMemoryBarriers.data();

    vkCmdPipelineBarrier2(cmd, &depInfo);

    m_MemoryBarrier.accessesBefore.clear();
    m_MemoryBarrier.accessesAfter.clear();
    m_ImageBarriers.clear();
    m_BufferBarriers.clear();

    return *this;
}

BarrierBuilder& BarrierBuilder::AddMemoryBarrier(std::vector<AccessType>&& accessesBefore,
                                                 std::vector<AccessType>&& accessesAfter)
{
    m_MemoryBarrier.accessesBefore = std::move(accessesBefore);
    m_MemoryBarrier.accessesAfter = std::move(accessesAfter);

    return *this;
}

BarrierBuilder& BarrierBuilder::AddImageBarrier(const Image& image,
                                                std::vector<AccessType>&& accessesBefore,
                                                std::vector<AccessType>&& accessesAfter)
{
    assert(!image.IsNull());

    m_ImageBarriers.emplace_back(image.GetImage(),
                                 EntireImageSubresourceRange(DetermineImageAspectFlagsFromFormat(image.GetFormat())),
                                 std::move(accessesBefore),
                                 std::move(accessesAfter),
                                 ImageLayout::Optimal,
                                 ImageLayout::Optimal,
                                 0,
                                 VK_QUEUE_FAMILY_IGNORED,
                                 VK_QUEUE_FAMILY_IGNORED);

    return *this;
}

BarrierBuilder& BarrierBuilder::AddBufferBarrier(const Buffer& buffer,
                                                 std::vector<AccessType>&& accessesBefore,
                                                 std::vector<AccessType>&& accessesAfter)
{
    assert(!buffer.IsNull());

    m_BufferBarriers.emplace_back(buffer.GetVkHandle(),
                                  0,
                                  VK_WHOLE_SIZE,
                                  std::move(accessesBefore),
                                  std::move(accessesAfter),
                                  VK_QUEUE_FAMILY_IGNORED,
                                  VK_QUEUE_FAMILY_IGNORED);

    return *this;
}

void BarrierBuilder::GetVulkanMemoryBarrier(const MemoryBarrier& barrier, VkMemoryBarrier2& vkBarrierOut)
{
    vkBarrierOut.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    vkBarrierOut.pNext = nullptr;
    vkBarrierOut.srcAccessMask = VK_ACCESS_2_NONE;
    vkBarrierOut.dstAccessMask = VK_ACCESS_2_NONE;

    for (uint32_t i = 0; i < barrier.accessesBefore.size(); ++i)
    {
        AccessType accessBefore = barrier.accessesBefore[i];
        const AccessInfo& accessBeforeInfo = AccessTypeMap[static_cast<uint64_t>(accessBefore)];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < THSVS_END_OF_READ_ACCESS || barrier.accessesBefore.size() == 1);
#endif

        vkBarrierOut.srcStageMask |= accessBeforeInfo.stageMask;
        vkBarrierOut.srcAccessMask |= accessBeforeInfo.accessMask;
    }

    for (uint32_t i = 0; i < barrier.accessesAfter.size(); ++i)
    {
        AccessType accessAfter = barrier.accessesAfter[i];
        const AccessInfo& accessAfterInfo = AccessTypeMap[static_cast<uint32_t>(accessAfter)];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(static_cast<uint32_t>(nextAccess) < static_cast<uint32_t>(AccessType::NumOfAccessTypes));
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < THSVS_END_OF_READ_ACCESS || barrier.accessesAfter.size() == 1);
#endif
        vkBarrierOut.dstStageMask |= accessAfterInfo.stageMask;
        vkBarrierOut.dstAccessMask |= accessAfterInfo.accessMask;
    }
}

void BarrierBuilder::GetVulkanBufferMemoryBarrier(const BufferBarrier& barrier, VkBufferMemoryBarrier2& vkBarrierOut)
{
    vkBarrierOut.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    vkBarrierOut.pNext = nullptr;
    vkBarrierOut.srcAccessMask = VK_ACCESS_2_NONE;
    vkBarrierOut.dstAccessMask = VK_ACCESS_2_NONE;
    vkBarrierOut.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
    vkBarrierOut.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
    vkBarrierOut.buffer = barrier.buffer;
    vkBarrierOut.offset = barrier.offset;
    vkBarrierOut.size = barrier.size;

#ifdef THSVS_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER
    assert(barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex);
#endif

    for (uint32_t i = 0; i < barrier.accessesBefore.size(); ++i)
    {
        AccessType accessBefore = barrier.accessesBefore[i];
        const AccessInfo& accessBeforeInfo = AccessTypeMap[static_cast<uint64_t>(accessBefore)];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < THSVS_END_OF_READ_ACCESS || barrier.accessesBefore.size() == 1);
#endif

        vkBarrierOut.srcStageMask |= accessBeforeInfo.stageMask;
        vkBarrierOut.srcAccessMask |= accessBeforeInfo.accessMask;
    }

    for (uint32_t i = 0; i < barrier.accessesAfter.size(); ++i)
    {
        AccessType accessAfter = barrier.accessesAfter[i];
        const AccessInfo& accessAfterInfo = AccessTypeMap[static_cast<uint64_t>(accessAfter)];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < THSVS_END_OF_READ_ACCESS || barrier.accessesAfter.size() == 1);
#endif

        vkBarrierOut.dstStageMask |= accessAfterInfo.stageMask;
        vkBarrierOut.dstAccessMask |= accessAfterInfo.accessMask;
    }
}

void BarrierBuilder::GetVulkanImageMemoryBarrier(const ImageBarrier& barrier, VkImageMemoryBarrier2& vkBarrierOut)
{
    vkBarrierOut.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    vkBarrierOut.pNext = nullptr;
    vkBarrierOut.srcAccessMask = VK_ACCESS_2_NONE;
    vkBarrierOut.dstAccessMask = VK_ACCESS_2_NONE;
    vkBarrierOut.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
    vkBarrierOut.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
    vkBarrierOut.image = barrier.image;
    vkBarrierOut.subresourceRange = barrier.subresourceRange;
    vkBarrierOut.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkBarrierOut.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (uint32_t i = 0; i < barrier.accessesBefore.size(); ++i)
    {
        AccessType accessBefore = barrier.accessesBefore[i];
        const AccessInfo& accessBeforeInfo = AccessTypeMap[static_cast<uint32_t>(accessBefore)];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the previous access index is a valid range for the lookup
        assert(prevAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(prevAccess < THSVS_END_OF_READ_ACCESS || barrier.accessesBefore.size() == 1);
#endif

        vkBarrierOut.srcStageMask |= accessBeforeInfo.stageMask;
        vkBarrierOut.srcAccessMask |= accessBeforeInfo.accessMask;

        if (barrier.discardContents == VK_TRUE)
        {
            vkBarrierOut.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
        else
        {
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

            switch (barrier.prevLayout)
            {
            case ImageLayout::General:
                if (accessBefore == AccessType::Present)
                    layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                else
                    layout = VK_IMAGE_LAYOUT_GENERAL;
                break;
            case ImageLayout::Optimal:
                layout = accessBeforeInfo.imageLayout;
                break;
            case ImageLayout::GeneralAndPresentation:
                layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                break;
            }

#ifdef THSVS_ERROR_CHECK_MIXED_IMAGE_LAYOUT
            assert(vkBarrierOut.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED || vkBarrierOut.oldLayout == layout);
#endif
            vkBarrierOut.oldLayout = layout;
        }
    }

    for (uint32_t i = 0; i < barrier.accessesAfter.size(); ++i)
    {
        AccessType accessAfter = barrier.accessesAfter[i];
        const AccessInfo& accessAfterInfo = AccessTypeMap[static_cast<uint32_t>(accessAfter)];

#ifdef THSVS_ERROR_CHECK_ACCESS_TYPE_IN_RANGE
        // Asserts that the next access index is a valid range for the lookup
        assert(nextAccess < THSVS_NUM_ACCESS_TYPES);
#endif

#ifdef THSVS_ERROR_CHECK_POTENTIAL_HAZARD
        // Asserts that the access is a read, else it's a write and it should appear on its own.
        assert(nextAccess < THSVS_END_OF_READ_ACCESS || barrier.accessesAfter.size() == 1);
#endif

        vkBarrierOut.dstStageMask |= accessAfterInfo.stageMask;
        vkBarrierOut.dstAccessMask |= accessAfterInfo.accessMask;

        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        switch (barrier.nextLayout)
        {
        case ImageLayout::General:
            if (accessAfter == AccessType::Present)
                layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            else
                layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case ImageLayout::Optimal:
            layout = accessAfterInfo.imageLayout;
            break;
        case ImageLayout::GeneralAndPresentation:
            layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        }

#ifdef THSVS_ERROR_CHECK_MIXED_IMAGE_LAYOUT
        assert(vkBarrierOut.newLayout == VK_IMAGE_LAYOUT_UNDEFINED || vkBarrierOut.newLayout == layout);
#endif
        vkBarrierOut.newLayout = layout;
    }

#ifdef THSVS_ERROR_CHECK_COULD_USE_GLOBAL_BARRIER
    assert(vkBarrierOut.newLayout != vkBarrierOut.oldLayout
           || vkBarrierOut.srcQueueFamilyIndex != vkBarrierOut.dstQueueFamilyIndex);
#endif
}

} // namespace Grace
