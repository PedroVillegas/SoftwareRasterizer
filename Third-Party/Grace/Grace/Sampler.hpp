#pragma once

#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

class Device;

/// Description used to create a Sampler object
struct GRACE_EXPORT SamplerDesc
{
    /// Specifies the minification filter to use when sampling an image
    VkFilter minFilter;
    /// Specifies the magnification filter to use when sampling an image
    VkFilter magFilter;
    /// Specifies the behavior of sampling with image coordinates outside the image
    VkSamplerAddressMode addressMode;
    /// Specifies the mipmap mode to use when sampling an image
    VkSamplerMipmapMode mipmapMode;
};

/// @brief `VkSampler` objects are required to read image data
/// and apply filtering and other transformations for the shader.
class GRACE_EXPORT Sampler
{
public:
    ~Sampler();
    Sampler() = default;
    Sampler(Device* pDevice, const SamplerDesc& desc);

    // Copy constructions/assignments are prohibited to stop destructor trying to
    // destroy the same VkSampler handle more than once
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    GRACE_NODISCARD bool IsNull() const;

    GRACE_NODISCARD VkSampler GetVkHandle() const;

    /// Sets index to resource in bindless array of Samplers for access on GPU.
    void SetSamplerId(const uint32_t id);

    /// @returns Index to resource in bindless array of Samplers for access on GPU.
    GRACE_NODISCARD uint32_t GetSamplerId() const;

private:
    Device* m_Device = nullptr;
    VkSampler m_Sampler = nullptr;
    uint32_t m_SamplerId = 0;
};

} // namespace Grace
