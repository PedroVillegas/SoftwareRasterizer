#pragma once

#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

class Device;

struct GRACE_EXPORT FenceDesc
{
    const char* name = "";
    VkFenceCreateFlags createFlags = 0;
};

class GRACE_EXPORT Fence
{
public:
    ~Fence();
    Fence() = default;
    Fence(Device* pDevice, const FenceDesc& desc);

    // Copy constructions/assignments are prohibited to stop destructor trying to
    // destroy the same VkFence handle more than once
    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    Fence(Fence&& other) noexcept;
    Fence& operator=(Fence&& other) noexcept;

    GRACE_NODISCARD bool IsNull() const;

    GRACE_NODISCARD const VkFence& GetVkFence() const;

private:
    Device* m_pDevice = nullptr;
    VkFence m_Fence = VK_NULL_HANDLE;
};

} // namespace Grace
