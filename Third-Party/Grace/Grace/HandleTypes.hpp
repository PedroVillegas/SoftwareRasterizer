#pragma once

#include <Grace/GraceExport.h>

namespace Grace
{

namespace SemaphoreType
{
    struct Binary;
    struct Timeline;
}

constexpr uint32_t INVALID_HANDLE = ~0U;
constexpr uint32_t INVALID_VALIDATOR = ~0U;

template <typename ResourceType>
struct Handle;

GRACE_DEFINE_RESOURCE_HANDLE(Buffer);
GRACE_DEFINE_RESOURCE_HANDLE(Image);
GRACE_DEFINE_RESOURCE_HANDLE(Sampler);
GRACE_DEFINE_RESOURCE_HANDLE(Pipeline);
GRACE_DEFINE_RESOURCE_HANDLE(PipelineLayout);
GRACE_DEFINE_RESOURCE_HANDLE(Fence);
GRACE_DEFINE_TEMPLATED_RESOURCE_HANDLE(Semaphore, SemaphoreType::Binary, BinarySemaphore);
GRACE_DEFINE_TEMPLATED_RESOURCE_HANDLE(Semaphore, SemaphoreType::Timeline, TimelineSemaphore);

template <typename ResourceType>
struct GRACE_EXPORT Handle
{
    Handle() = default;

    Handle(uint32_t UUID, uint32_t Validator) : handle(UUID), validator(Validator)
    {
    }

    GRACE_NODISCARD bool HasValidHandle() const
    {
        return handle != INVALID_HANDLE;
    }

    uint32_t handle = INVALID_HANDLE;
    uint32_t validator = INVALID_VALIDATOR;
};

} // namespace Grace
