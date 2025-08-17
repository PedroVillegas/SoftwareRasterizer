#include "CommandGroup.hpp"

#include <Grace/DebugReporter.hpp>
#include <Grace/Context.hpp>
#include <Grace/HelperFunctions.hpp>

#include <cassert>

namespace Grace
{

CommandGroupAllocator::CommandGroupAllocator(Device* device) : m_pDevice(device)
{
}

CommandPool* CommandGroupAllocator::GetOrAllocateCommandPool(QueueFamily queueFamily, const char* name)
{
    assert(static_cast<uint32_t>(queueFamily) <= static_cast<uint32_t>(QueueFamily::Graphics));

    // If no pools are free, allocate new pool
    std::deque<CommandPool>& allocatedPoolsOfQueueFamily =
        m_AllCommandPoolsAllocated[static_cast<uint32_t>(queueFamily)];
    if (m_FreeCommandPools.empty())
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.pNext = nullptr;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_pDevice->GetQueueFamilyIndex(queueFamily);

        VkCommandPool allocatedCmdPool = {};
        DebugReporter::Check(vkCreateCommandPool(m_pDevice->GetVkHandle(), &poolInfo, nullptr, &allocatedCmdPool));

        allocatedPoolsOfQueueFamily.emplace_back(m_pDevice, allocatedCmdPool, queueFamily);
        m_FreeCommandPools.push(&allocatedPoolsOfQueueFamily.back());
    }

    assert(!m_FreeCommandPools.empty());
    CommandPool* ret = m_FreeCommandPools.front();
    m_FreeCommandPools.pop();

    AssignDebugName<VkCommandPool>(m_pDevice->GetVkHandle(), ret->GetVkCommandPool(), name);

    return ret;
}

void CommandGroupAllocator::ReturnCommandPool(CommandPool* commandPool)
{
    m_FreeCommandPools.push(commandPool);
}

void CommandGroupAllocator::FreeCommandPool()
{
}

void CommandGroupAllocator::FreeCommandBuffer()
{
}

CommandPool::~CommandPool()
{
    if (m_CommandPool != nullptr)
    {
        vkDestroyCommandPool(m_Device->GetVkHandle(), m_CommandPool, nullptr);
    }
}

CommandPool::CommandPool(Device* pDevice, VkCommandPool commandPool, QueueFamily queueFamily)
    : m_Device(pDevice), m_CommandPool(commandPool), m_QueueFamily(queueFamily)
{
}

CommandPool::CommandPool(CommandPool&& other) noexcept
    : m_Device(other.m_Device), m_CommandPool(other.m_CommandPool), m_QueueFamily(other.m_QueueFamily),
      m_CommandBuffers(std::move(other.m_CommandBuffers)), m_CommandBuffersInUse(other.m_CommandBuffersInUse)
{
    other.m_CommandPool = nullptr;
}

CommandPool& CommandPool::operator=(CommandPool&& other) noexcept
{
    m_Device = other.m_Device;
    m_CommandPool = other.m_CommandPool;
    m_QueueFamily = other.m_QueueFamily;
    m_CommandBuffers = std::move(other.m_CommandBuffers);
    m_CommandBuffersInUse = other.m_CommandBuffersInUse;
    other.m_CommandPool = nullptr;

    return *this;
}

void CommandPool::Reset()
{
    vkResetCommandPool(m_Device->GetVkHandle(), m_CommandPool, 0);
}

CommandBuffer CommandPool::GetOrAllocateCommandBuffer()
{
    if (m_CommandBuffers.size() == m_CommandBuffersInUse)
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer allocated = {};
        DebugReporter::Check(vkAllocateCommandBuffers(m_Device->GetVkHandle(), &allocInfo, &allocated));

        m_CommandBuffers.push_back(allocated);
    }

    return { m_Device, m_CommandBuffers[m_CommandBuffersInUse++], m_QueueFamily, m_Device->GetQueryManagerPtr() };
}

QueueFamily CommandPool::GetQueueFamily() const
{
    return m_QueueFamily;
}

VkCommandPool CommandPool::GetVkCommandPool() const
{
    return m_CommandPool;
}

CommandBuffer::CommandBuffer(Device* pDevice, VkCommandBuffer commandBuffer, QueueFamily queueFamily, QueryManager* pQueryMgr)
    : m_pDevice(pDevice), m_CmdBuffer(commandBuffer), m_QueueFamily(queueFamily), m_pQueryMgr(pQueryMgr)
{
}

bool CommandBuffer::IsNull() const
{
    return m_CmdBuffer == nullptr;
}

const VkCommandBuffer& CommandBuffer::GetVkCommandBuffer() const
{
    return m_CmdBuffer;
}

void CommandBuffer::Reset(VkCommandBufferResetFlags resetFlags) const
{
    DebugReporter::Check(vkResetCommandBuffer(m_CmdBuffer, resetFlags));
}

void CommandBuffer::BeginRecording(VkCommandBufferUsageFlags usageFlags,
                                   const VkCommandBufferInheritanceInfo* pInheritanceInfo) const
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = usageFlags;
    beginInfo.pInheritanceInfo = pInheritanceInfo;

    DebugReporter::Check(vkBeginCommandBuffer(m_CmdBuffer, &beginInfo));
}

void CommandBuffer::EndRecording() const
{
    DebugReporter::Check(vkEndCommandBuffer(m_CmdBuffer));
}

void CommandBuffer::BindPipeline(PipelineHandle pipeline, VkPipelineBindPoint bindPoint) const
{
    const Pipeline& p = m_pDevice->GetPipeline(pipeline);
    assert(!p.IsNull());

    vkCmdBindPipeline(m_CmdBuffer, bindPoint, p.GetVkHandle());
}

void CommandBuffer::BindDescriptorSets(VkPipelineBindPoint pipelineBindPoint,
                                       PipelineLayoutHandle layout,
                                       uint32_t firstSet,
                                       const std::vector<VkDescriptorSet>& descriptorSets) const
{
    const PipelineLayout& pl = m_pDevice->GetPipelineLayout(layout);
    assert(!pl.IsNull());
    vkCmdBindDescriptorSets(m_CmdBuffer,
                            pipelineBindPoint,
                            pl.GetVkPipelineLayout(),
                            firstSet,
                            static_cast<uint32_t>(descriptorSets.size()),
                            descriptorSets.data(),
                            0,
                            nullptr);
}

void CommandBuffer::BeginDynamicRendering(const DynamicRenderingDesc& desc) const
{
    assert(!desc.colorAttachments.empty());
    assert(desc.renderArea.extent.width > 0 && desc.renderArea.extent.height > 0);

    VkRenderingInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.flags = desc.flags;
    renderInfo.renderArea = VkRect2D(desc.renderArea.offset, desc.renderArea.extent);
    renderInfo.layerCount = desc.layerCount;
    renderInfo.viewMask = desc.viewMask;
    renderInfo.colorAttachmentCount = static_cast<uint32_t>(desc.colorAttachments.size());
    renderInfo.pColorAttachments = desc.colorAttachments.data();
    renderInfo.pDepthAttachment = desc.depthAttachments.data();
    renderInfo.pStencilAttachment = desc.stencilAttachments.data();

    vkCmdBeginRendering(m_CmdBuffer, &renderInfo);
}

void CommandBuffer::EndDynamicRendering() const
{
    vkCmdEndRendering(m_CmdBuffer);
}

void CommandBuffer::SetViewport(const std::vector<VkViewport>& viewports, uint32_t firstViewport) const
{
    assert(!viewports.empty());
    vkCmdSetViewport(m_CmdBuffer, firstViewport, static_cast<uint32_t>(viewports.size()), viewports.data());
}

void CommandBuffer::SetScissor(const std::vector<VkRect2D>& scissors, uint32_t firstScissor) const
{
    assert(!scissors.empty());
    vkCmdSetScissor(m_CmdBuffer, firstScissor, static_cast<uint32_t>(scissors.size()), scissors.data());
}

void CommandBuffer::PushConstants(PipelineLayoutHandle layout, uint32_t size, const void* data) const
{
    const PipelineLayout& pl = m_pDevice->GetPipelineLayout(layout);
    assert(!pl.IsNull());
    assert(size <= 128);
    vkCmdPushConstants(m_CmdBuffer, pl.GetVkPipelineLayout(), VK_SHADER_STAGE_ALL, 0, size, data);
}

void CommandBuffer::BeginDebugLabel(const char* label, const std::array<float, 4>& color) const
{
    VkDebugUtilsLabelEXT labelInfo = {};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pNext = nullptr;
    labelInfo.pLabelName = label;
    labelInfo.color[0] = color[0];
    labelInfo.color[1] = color[1];
    labelInfo.color[2] = color[2];
    labelInfo.color[3] = color[3];

    vkCmdBeginDebugUtilsLabelEXT_Meta(m_CmdBuffer, &labelInfo);
}

void CommandBuffer::InsertDebugLabel(const char* label, const std::array<float, 4>& color) const
{
    VkDebugUtilsLabelEXT labelInfo = {};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pNext = nullptr;
    labelInfo.pLabelName = label;
    labelInfo.color[0] = color[0];
    labelInfo.color[1] = color[1];
    labelInfo.color[2] = color[2];
    labelInfo.color[3] = color[3];

    vkCmdInsertDebugUtilsLabelEXT_Meta(m_CmdBuffer, &labelInfo);
}

void CommandBuffer::EndDebugLabel() const
{
    vkCmdEndDebugUtilsLabelEXT_Meta(m_CmdBuffer);
}

void CommandBuffer::AddBufferBarrier(BufferHandle buffer,
                                     std::vector<AccessType>&& accessesBefore,
                                     std::vector<AccessType>&& accessesAfter)
{
    const Buffer& buf = m_pDevice->GetBuffer(buffer);
    m_BarrierBuilder.AddBufferBarrier(buf, std::move(accessesBefore), std::move(accessesAfter));
}

void CommandBuffer::AddImageBarrier(ImageHandle image,
                                    std::vector<AccessType>&& accessesBefore,
                                    std::vector<AccessType>&& accessesAfter)
{
    const Image& img = m_pDevice->GetImage(image);
    m_BarrierBuilder.AddImageBarrier(img, std::move(accessesBefore), std::move(accessesAfter));
}

void CommandBuffer::AddMemoryBarrier(std::vector<AccessType>&& accessesBefore, std::vector<AccessType>&& accessesAfter)
{
    m_BarrierBuilder.AddMemoryBarrier(std::move(accessesBefore), std::move(accessesAfter));
}

void CommandBuffer::PipelineBarrier()
{
    m_BarrierBuilder.PipelineBarrier(m_CmdBuffer);
}

void CommandBuffer::Dispatch(uint32_t x, uint32_t y, uint32_t z) const
{
    vkCmdDispatch(m_CmdBuffer, x, y, z);
}

void CommandBuffer::DispatchIndirect(BufferHandle buffer, uint64_t offset) const
{
    const Buffer& buf = m_pDevice->GetBuffer(buffer);
    assert(!buf.IsNull());
    vkCmdDispatchIndirect(m_CmdBuffer, buf.GetVkHandle(), offset);
}

void CommandBuffer::BlitImage(const VkBlitImageInfo2& blitInfo) const
{
    vkCmdBlitImage2(m_CmdBuffer, &blitInfo);
}

void CommandBuffer::CopyBufferToImage(BufferHandle buffer,
                                      ImageHandle image,
                                      VkImageLayout dstLayout,
                                      const std::vector<VkBufferImageCopy>& regions) const
{
    const Buffer& buf = m_pDevice->GetBuffer(buffer);
    const Image& img = m_pDevice->GetImage(image);
    assert(!buf.IsNull());
    assert(!img.IsNull());
    assert(!regions.empty());
    vkCmdCopyBufferToImage(m_CmdBuffer,
                           buf.GetVkHandle(),
                           img.GetImage(),
                           dstLayout,
                           static_cast<uint32_t>(regions.size()),
                           regions.data());
}

void CommandBuffer::CopyBuffer(BufferHandle srcBuffer,
                               BufferHandle dstBuffer,
                               const std::vector<VkBufferCopy>& regions) const
{
    const Buffer& srcbuf = m_pDevice->GetBuffer(srcBuffer);
    const Buffer& dstbuf = m_pDevice->GetBuffer(dstBuffer);

    assert(!srcbuf.IsNull());
    assert(!dstbuf.IsNull());
    assert(!regions.empty());
    vkCmdCopyBuffer(m_CmdBuffer,
                    srcbuf.GetVkHandle(),
                    dstbuf.GetVkHandle(),
                    static_cast<uint32_t>(regions.size()),
                    regions.data());
}

void CommandBuffer::FillBuffer(BufferHandle buffer, uint32_t data, VkDeviceSize offset, VkDeviceSize size) const
{
    const Buffer& buf = m_pDevice->GetBuffer(buffer);
    vkCmdFillBuffer(m_CmdBuffer, buf.GetVkHandle(), offset, size, data);
}

void CommandBuffer::WriteTimestamp(const char* name,
                                   VkPipelineStageFlags2 stage,
                                   uint32_t frameIndex) const
{
    const TimestampQueryGroup& qg = m_pQueryMgr->GetQueryGroup<QueryType::Timestamp>();
    const uint32_t query = m_pQueryMgr->AddQuery<QueryType::Timestamp>(name);

    const uint32_t offset = frameIndex * (qg.GetRange() - 1);

    vkCmdWriteTimestamp2(m_CmdBuffer, stage, qg.GetVkQueryPool(), offset + query);
}

void CommandBuffer::ClearColorImage(ImageHandle image,
                                    const VkClearColorValue& color,
                                    const std::vector<VkImageSubresourceRange>& ranges) const
{
    const Image& img = m_pDevice->GetImage(image);
    assert(!img.IsNull());
    vkCmdClearColorImage(m_CmdBuffer,
                         img.GetImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &color,
                         static_cast<uint32_t>(ranges.size()),
                         ranges.data());
}

void CommandBuffer::BindIndexBuffer(BufferHandle buffer, VkDeviceSize offset, VkIndexType indexType) const
{
    const Buffer& buf = m_pDevice->GetBuffer(buffer);
    assert(!buf.IsNull());
    vkCmdBindIndexBuffer(m_CmdBuffer, buf.GetVkHandle(), offset, indexType);
}

void CommandBuffer::Draw(uint32_t vertexCount,
                         uint32_t instanceCount,
                         uint32_t firstVertex,
                         uint32_t firstInstance) const
{
    vkCmdDraw(m_CmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void CommandBuffer::DrawIndexed(uint32_t indexCount,
                                uint32_t instanceCount,
                                uint32_t firstIndex,
                                int32_t vertexOffset,
                                uint32_t firstInstance) const
{
    vkCmdDrawIndexed(m_CmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void CommandBuffer::DrawIndexedIndirect(BufferHandle buffer,
                                        VkDeviceSize offset,
                                        uint32_t drawCount,
                                        uint32_t stride) const
{
    const Buffer& buf = m_pDevice->GetBuffer(buffer);
    assert(!buf.IsNull());
    vkCmdDrawIndexedIndirect(m_CmdBuffer, buf.GetVkHandle(), offset, drawCount, stride);
}

} // namespace Grace
