#pragma once

#include <concepts>

#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>
#include <Grace/DebugReporter.hpp>
#include <Grace/HelperFunctions.hpp>

namespace Grace
{

class Device;

namespace SemaphoreType
{

struct SemaphoreTypeBase
{
};

struct GRACE_EXPORT Binary : SemaphoreTypeBase
{
};

struct GRACE_EXPORT Timeline : SemaphoreTypeBase
{
};

} // namespace SemaphoreType

struct GRACE_EXPORT SemaphoreDesc
{
    const char* name = "";
    uint64_t initialValue = 0;
};

template <typename SemTy>
class GRACE_EXPORT Semaphore
{
    static_assert(std::derived_from<SemTy, SemaphoreType::SemaphoreTypeBase>,
                  "Semaphore type must be derived from SemaphoreType::SemaphoreTypeBase!");

public:
    ~Semaphore();
    Semaphore() = default;
    Semaphore(Device* pDevice, const SemaphoreDesc& desc);

    // Copy constructions/assignments are prohibited to stop destructor trying to
    // destroy the same VkSemaphore handle more than once
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    Semaphore(Semaphore&& other) noexcept;

    Semaphore& operator=(Semaphore&& other) noexcept;

    GRACE_NODISCARD bool IsNull() const;

    GRACE_NODISCARD const VkSemaphore& GetVkSemaphore() const;

private:
    Device* m_pDevice = nullptr;
    VkSemaphore m_Semaphore = VK_NULL_HANDLE;
};

using BinarySemaphore = Semaphore<SemaphoreType::Binary>;
using TimelineSemaphore = Semaphore<SemaphoreType::Timeline>;

} // namespace Grace
