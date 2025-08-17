#include "Buffer.hpp"

#include "HelperFunctions.hpp"

#include <Grace/Context.hpp>
#include <Grace/DebugReporter.hpp>

#include <cassert>

namespace Grace
{

bool Buffer::IsNull() const
{
    return (m_Buffer == nullptr || m_Allocation == nullptr);
}

VkBuffer Buffer::GetVkHandle() const
{
    return m_Buffer;
}

VkDeviceAddress Buffer::GetBDA() const
{
    assert(m_DeviceAddress != 0);
    return m_DeviceAddress;
}

VmaAllocation Buffer::GetAllocation() const
{
    return m_Allocation;
}

VmaAllocationInfo2 Buffer::GetAllocationInfo() const
{
    VmaAllocationInfo2 info = {};
    vmaGetAllocationInfo2(m_Device->GetVmaHandle(), m_Allocation, &info);
    return info;
}

Buffer::Buffer(Device* pDevice, const BufferDesc& desc) : m_Device(pDevice)
{
    assert(!m_Device->IsNull());

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = desc.size;
    bufferInfo.usage = desc.usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaAllocInfo.flags = desc.allocFlags;

    DebugReporter::Check(
        vmaCreateBuffer(m_Device->GetVmaHandle(), &bufferInfo, &vmaAllocInfo, &m_Buffer, &m_Allocation, nullptr));
    assert(m_Buffer != nullptr);
    AssignDebugName<VkBuffer>(m_Device->GetVkHandle(), m_Buffer, desc.name);

    if ((desc.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0)
    {
        VkBufferDeviceAddressInfo deviceAddressInfo = {};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        deviceAddressInfo.pNext = nullptr;
        deviceAddressInfo.buffer = m_Buffer;

        m_DeviceAddress = vkGetBufferDeviceAddress(m_Device->GetVkHandle(), &deviceAddressInfo);
    }

    if (desc.data != nullptr)
    {
        VkBufferCreateInfo stagingBufferInfo = {};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.pNext = nullptr;
        stagingBufferInfo.size = desc.size;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingBufferAllocationInfo = {};
        stagingBufferAllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingBufferAllocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        VmaAllocation stagingBufferAllocation = nullptr;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        DebugReporter::Check(vmaCreateBuffer(m_Device->GetVmaHandle(),
                                             &stagingBufferInfo,
                                             &stagingBufferAllocationInfo,
                                             &stagingBuffer,
                                             &stagingBufferAllocation,
                                             nullptr));
        vmaCopyMemoryToAllocation(m_Device->GetVmaHandle(), desc.data, stagingBufferAllocation, 0, desc.size);

        const CommandBuffer& cmd = m_Device->BeginSingleTimeCommands();
        const std::string debugLabel = desc.name + std::string(" | Data upload/Mip Gen");
        cmd.BeginDebugLabel(debugLabel.c_str(), { 1.0F, 1.0F, 1.0F, 1.0F });

        // Copy indices data to staging buffer, then copy staging buffer to index buffer
        VkBufferCopy2 copyRegion = {};
        copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        copyRegion.pNext = nullptr;
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = desc.size;

        VkCopyBufferInfo2 copyBufferInfo = {};
        copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        copyBufferInfo.pNext = nullptr;
        copyBufferInfo.srcBuffer = stagingBuffer;
        copyBufferInfo.dstBuffer = m_Buffer;
        copyBufferInfo.regionCount = 1;
        copyBufferInfo.pRegions = &copyRegion;

        vkCmdCopyBuffer2(cmd.GetVkCommandBuffer(), &copyBufferInfo);

        cmd.EndDebugLabel();
        pDevice->EndAndSubmitSingleTimeCommands();
        vmaDestroyBuffer(m_Device->GetVmaHandle(), stagingBuffer, stagingBufferAllocation);
    }
}

Buffer::~Buffer()
{
    if (m_Device != nullptr)
    {
        vmaDestroyBuffer(m_Device->GetVmaHandle(), m_Buffer, m_Allocation);
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_Device(other.m_Device), m_Buffer(other.m_Buffer), m_Allocation(other.m_Allocation),
      m_DeviceAddress(other.m_DeviceAddress)
{
    other.m_Device = VK_NULL_HANDLE;
    other.m_Buffer = VK_NULL_HANDLE;
    other.m_Allocation = VK_NULL_HANDLE;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (m_Device != nullptr)
    {
        vmaDestroyBuffer(m_Device->GetVmaHandle(), m_Buffer, m_Allocation);
    }

    m_Device = other.m_Device;
    m_Buffer = other.m_Buffer;
    m_Allocation = other.m_Allocation;
    m_DeviceAddress = other.m_DeviceAddress;
    other.m_Device = VK_NULL_HANDLE;
    other.m_Buffer = VK_NULL_HANDLE;
    other.m_Allocation = VK_NULL_HANDLE;

    return *this;
}

} // namespace Grace
