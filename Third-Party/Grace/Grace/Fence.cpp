#include "Fence.hpp"

#include <Grace/Device.hpp>
#include <Grace/HelperFunctions.hpp>

Grace::Fence::~Fence()
{
    if (m_Fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_pDevice->GetVkHandle(), m_Fence, nullptr);
    }
}

Grace::Fence::Fence(Device* pDevice, const FenceDesc& desc) : m_pDevice(pDevice)
{
    VkFenceCreateInfo cinfo = {};
    cinfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    cinfo.pNext = nullptr;
    cinfo.flags = desc.createFlags;
    DebugReporter::Check(vkCreateFence(m_pDevice->GetVkHandle(), &cinfo, nullptr, &m_Fence));

    AssignDebugName<VkFence>(m_pDevice->GetVkHandle(), m_Fence, desc.name);
}

Grace::Fence::Fence(Fence&& other) noexcept : m_pDevice(other.m_pDevice), m_Fence(other.m_Fence)
{
    other.m_pDevice = nullptr;
    other.m_Fence = VK_NULL_HANDLE;
}

Grace::Fence& Grace::Fence::operator=(Fence&& other) noexcept
{
    if (m_Fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_pDevice->GetVkHandle(), m_Fence, nullptr);
    }

    m_pDevice = other.m_pDevice;
    m_Fence = other.m_Fence;
    other.m_Fence = VK_NULL_HANDLE;

    return *this;
}

bool Grace::Fence::IsNull() const
{
    return m_Fence == VK_NULL_HANDLE;
}

const VkFence& Grace::Fence::GetVkFence() const
{
    return m_Fence;
}