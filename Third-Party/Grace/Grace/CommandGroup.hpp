#pragma once

#include <array>
#include <queue>
#include <cassert>

#include <vulkan/vulkan.h>
#include <Grace/PipelineGroup.hpp>
#include <Grace/QueryManager.hpp>
#include <Grace/HandleTypes.hpp>
#include <Grace/Types.hpp>
#include <Grace/SyncGroup.hpp>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

class Device;

enum class QueueFamily : uint32_t
{
    /// Queue supporting transfer operations
    Transfer,
    /// Queue supporting transfer and compute pipeline operations
    Compute,
    /// Queue supporting transfer and graphics pipeline operations
    Graphics,
    /// Queue supporting present operations
    Present,
    /// Queue is undefined
    Undefined
};

class CommandBuffer;

class GRACE_EXPORT CommandPool
{
public:
    ~CommandPool();
    CommandPool(Device* pDevice, VkCommandPool commandPool, QueueFamily queueFamily);

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    CommandPool(CommandPool&& other) noexcept;
    CommandPool& operator=(CommandPool&& other) noexcept;

    void Reset();

    GRACE_NODISCARD CommandBuffer GetOrAllocateCommandBuffer();

    GRACE_NODISCARD QueueFamily GetQueueFamily() const;

    GRACE_NODISCARD VkCommandPool GetVkCommandPool() const;

private:
    Device* m_Device = nullptr;
    VkCommandPool m_CommandPool = nullptr;
    QueueFamily m_QueueFamily = QueueFamily::Undefined;
    std::vector<VkCommandBuffer> m_CommandBuffers = {};
    uint32_t m_CommandBuffersInUse = 0;
};

class GRACE_EXPORT CommandBuffer
{
public:
    ~CommandBuffer() = default;
    CommandBuffer() = default;
    CommandBuffer(Device* pDevice, VkCommandBuffer commandBuffer, QueueFamily queueFamily, QueryManager* pQueryMgr);

    CommandBuffer(const CommandBuffer&) = default;
    CommandBuffer& operator=(const CommandBuffer&) = default;

    CommandBuffer(CommandBuffer&&) noexcept = default;
    CommandBuffer& operator=(CommandBuffer&&) noexcept = default;

    GRACE_NODISCARD bool IsNull() const;

    GRACE_NODISCARD const VkCommandBuffer& GetVkCommandBuffer() const;

    /// UNIVERSAL OPS

    void Reset(VkCommandBufferResetFlags resetFlags = 0) const;

    void BeginRecording(VkCommandBufferUsageFlags usageFlags = 0,
                        const VkCommandBufferInheritanceInfo* pInheritanceInfo = nullptr) const;

    void EndRecording() const;

    void BindPipeline(PipelineHandle pipeline, VkPipelineBindPoint bindPoint) const;

    void BindDescriptorSets(VkPipelineBindPoint pipelineBindPoint,
                            PipelineLayoutHandle layout,
                            uint32_t firstSet,
                            const std::vector<VkDescriptorSet>& descriptorSets) const;

    void PushConstants(PipelineLayoutHandle layout, uint32_t size, const void* data) const;

    void BeginDebugLabel(const char* label, const std::array<float, 4>& color = { 0.6F, 0.6F, 0.6F, 1.0F }) const;

    void InsertDebugLabel(const char* label, const std::array<float, 4>& color = { 0.6F, 0.6F, 0.6F, 1.0F }) const;

    void EndDebugLabel() const;

    /// SYNC OPS

    void AddBufferBarrier(BufferHandle buffer,
                          std::vector<AccessType>&& accessesBefore,
                          std::vector<AccessType>&& accessesAfter);

    void AddImageBarrier(ImageHandle image,
                         std::vector<AccessType>&& accessesBefore,
                         std::vector<AccessType>&& accessesAfter);

    void AddMemoryBarrier(std::vector<AccessType>&& prevAccesses, std::vector<AccessType>&& nextAccesses);

    void PipelineBarrier();

    /// GRAPHICS OPS

    void BeginDynamicRendering(const DynamicRenderingDesc& desc) const;

    void EndDynamicRendering() const;

    void SetViewport(const std::vector<VkViewport>& viewports, uint32_t firstViewport = 0) const;

    void SetScissor(const std::vector<VkRect2D>& scissors, uint32_t firstScissor = 0) const;

    void BindIndexBuffer(BufferHandle buffer, VkDeviceSize offset, VkIndexType indexType) const;

    void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) const;

    void DrawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance) const;

    void DrawIndexedIndirect(BufferHandle buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const;

    /// COMPUTE OPS

    void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) const;

    void DispatchIndirect(BufferHandle buffer, uint64_t offset) const;

    /// TRANSFER OPS

    void BlitImage(const VkBlitImageInfo2& blitInfo) const;

    void ClearColorImage(ImageHandle image,
                         const VkClearColorValue& color,
                         const std::vector<VkImageSubresourceRange>& ranges) const;

    void CopyBufferToImage(BufferHandle buffer,
                           ImageHandle image,
                           VkImageLayout dstLayout,
                           const std::vector<VkBufferImageCopy>& regions) const;

    void CopyBuffer(BufferHandle srcBuffer, BufferHandle dstBuffer, const std::vector<VkBufferCopy>& regions) const;

    void
    FillBuffer(BufferHandle buffer, uint32_t data, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) const;

    /// QUERY OPS

    template <typename T>
    void ResetQueryPoolFullRange(uint32_t frameIndex)
    {
        assert(m_pQueryMgr);

        const QueryGroup<T>& qg = m_pQueryMgr->GetQueryGroup<T>();
        m_pQueryMgr->ResetQueryGroup<T>();

        uint32_t start = frameIndex * qg.GetRange();
        uint32_t end = qg.GetRange();

        vkCmdResetQueryPool(m_CmdBuffer, qg.GetVkQueryPool(), start, end);
    }

    template <typename T>
    void ResetQueryPool(uint32_t firstQuery, uint32_t queryCount, uint32_t frameIndex)
    {
        assert(m_pQueryMgr);

        const QueryGroup<T>& qg = m_pQueryMgr->GetQueryGroup<T>();
        assert(queryCount < qg.GetRange());

        uint32_t start = (frameIndex * qg.GetRange()) + firstQuery;
        uint32_t end = start + queryCount;

        vkCmdResetQueryPool(m_CmdBuffer, qg.GetVkQueryPool(), start, end);
    }

    template <typename T>
    void BeginQuery(const char* name,
                    uint32_t frameIndex,
                    QueryWriteFlags writeFlags = QueryWriteFlags::None,
                    VkQueryControlFlags controlFlags = 0)
    {
        QueryGroup<T>& qg = m_pQueryMgr->GetQueryGroup<T>();
        const uint32_t query = m_pQueryMgr->AddQuery<T>(name);

        const uint32_t offset = frameIndex * qg.GetRange();
        if (writeFlags == QueryWriteFlags::WriteIfPreviousResultIsAvailable)
        {
            if (qg.GetQueries()[offset + query + qg.GetValuesPerQuery()] == 0)
            {
                return;
            };
        }

        vkCmdBeginQuery(m_CmdBuffer, qg.GetVkQueryPool(), query, controlFlags);
    }

    template <typename T>
    void EndQuery(const char* name)
    {
        const QueryGroup<T>& qg = m_pQueryMgr->GetQueryGroup<T>();
        vkCmdEndQuery(m_CmdBuffer, qg.GetVkQueryPool(), qg.GetQueryOffset(name));
    }

    void WriteTimestamp(const char* name, VkPipelineStageFlags2 stage, uint32_t frameIndex) const;

private:
    Device* m_pDevice = nullptr;
    VkCommandBuffer m_CmdBuffer = {};
    BarrierBuilder m_BarrierBuilder = {};
    QueueFamily m_QueueFamily = QueueFamily::Undefined;
    QueryManager* m_pQueryMgr = nullptr;
};

class GRACE_EXPORT CommandGroupAllocator
{
public:
    ~CommandGroupAllocator() = default;
    CommandGroupAllocator() = default;
    explicit CommandGroupAllocator(Device* device);

    CommandGroupAllocator(const CommandGroupAllocator&) = delete;
    CommandGroupAllocator& operator=(const CommandGroupAllocator&) = delete;

    CommandGroupAllocator(CommandGroupAllocator&&) noexcept = delete;
    CommandGroupAllocator& operator=(CommandGroupAllocator&&) noexcept = delete;

    GRACE_NODISCARD CommandPool* GetOrAllocateCommandPool(QueueFamily queueFamily, const char* name);

    void ReturnCommandPool(CommandPool* commandPool);

    void FreeCommandPool();

    void FreeCommandBuffer();

private:
    Device* m_pDevice = nullptr;
    // Use std::deque here to prevent any pointer invalidations. No performance hit since
    // new CommandPools are strictly inserted/removed from either end
    std::array<std::deque<CommandPool>, static_cast<uint32_t>(QueueFamily::Undefined)> m_AllCommandPoolsAllocated = {};
    std::queue<CommandPool*> m_FreeCommandPools = {};
};

} // namespace Grace
