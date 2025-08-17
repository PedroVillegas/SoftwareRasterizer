#include "Sampler.hpp"

#include "Device.hpp"

#include <cassert>

#include <Grace/DebugReporter.hpp>

namespace Grace
{

Sampler::~Sampler()
{
    if (m_Sampler != nullptr)
    {
        vkDestroySampler(m_Device->GetVkHandle(), m_Sampler, nullptr);
    }
}

Sampler::Sampler(Device* pDevice, const SamplerDesc& desc) : m_Device(pDevice)
{
    assert(!m_Device->IsNull());

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext = nullptr;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.minLod = 0;
    samplerInfo.minFilter = desc.minFilter;
    samplerInfo.magFilter = desc.magFilter;
    samplerInfo.mipmapMode = desc.mipmapMode;
    samplerInfo.addressModeU = desc.addressMode;
    samplerInfo.addressModeV = desc.addressMode;
    samplerInfo.addressModeW = desc.addressMode;

    DebugReporter::Check(vkCreateSampler(m_Device->GetVkHandle(), &samplerInfo, nullptr, &m_Sampler));
}

Sampler::Sampler(Sampler&& other) noexcept : m_Device(other.m_Device), m_Sampler(other.m_Sampler)
{
    other.m_Sampler = nullptr;
}

Sampler& Sampler::operator=(Sampler&& other) noexcept
{
    if (m_Sampler != nullptr)
    {
        vkDestroySampler(m_Device->GetVkHandle(), m_Sampler, nullptr);
    }

    m_Device = other.m_Device;
    m_Sampler = other.m_Sampler;
    other.m_Sampler = nullptr;

    return *this;
}

bool Sampler::IsNull() const
{
    return m_Sampler == nullptr;
}

VkSampler Sampler::GetVkHandle() const
{
    return m_Sampler;
}

void Sampler::SetSamplerId(const uint32_t id)
{
    m_SamplerId = id;
}

uint32_t Sampler::GetSamplerId() const
{
    return m_SamplerId;
}

} // namespace Grace
