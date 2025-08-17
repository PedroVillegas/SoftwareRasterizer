#include "Semaphore.hpp"

#include <Grace/Device.hpp>

namespace Grace
{

template <typename SemTy>
Semaphore<SemTy>::~Semaphore()
{
    if (!IsNull())
    {
        vkDestroySemaphore(m_pDevice->GetVkHandle(), m_Semaphore, nullptr);
    }
}

template <typename SemTy>
Semaphore<SemTy>::Semaphore(Device* pDevice, const SemaphoreDesc& desc) : m_pDevice(pDevice)
{
    VkSemaphoreCreateInfo cinfo = {};
    cinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    cinfo.pNext = nullptr;
    cinfo.flags = 0;

    VkSemaphoreTypeCreateInfo tcinfo = {};
    if constexpr (std::is_same_v<SemTy, SemaphoreType::Timeline>)
    {
        tcinfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        tcinfo.pNext = nullptr;
        tcinfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        tcinfo.initialValue = desc.initialValue;
        cinfo.pNext = &tcinfo;
    }

    DebugReporter::Check(vkCreateSemaphore(m_pDevice->GetVkHandle(), &cinfo, nullptr, &m_Semaphore));
    AssignDebugName<VkSemaphore>(m_pDevice->GetVkHandle(), m_Semaphore, desc.name);
}

template <typename SemTy>
Semaphore<SemTy>::Semaphore(Semaphore&& other) noexcept : m_pDevice(other.m_pDevice), m_Semaphore(other.m_Semaphore)
{
    other.m_Semaphore = VK_NULL_HANDLE;
}

template <typename SemTy>
Semaphore<SemTy>& Semaphore<SemTy>::operator=(Semaphore&& other) noexcept
{
    m_pDevice = other.m_pDevice;
    m_Semaphore = other.m_Semaphore;
    other.m_Semaphore = VK_NULL_HANDLE;
    return *this;
}

template <typename SemTy>
bool Semaphore<SemTy>::IsNull() const
{
    return m_Semaphore == VK_NULL_HANDLE;
}

template <typename SemTy>
const VkSemaphore& Semaphore<SemTy>::GetVkSemaphore() const
{
    return m_Semaphore;
}

template class Semaphore<SemaphoreType::Binary>;
template class Semaphore<SemaphoreType::Timeline>;

} // namespace Grace
