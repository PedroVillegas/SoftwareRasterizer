#pragma once

#include <vector>
#include <queue>
#include <cassert>

#include <Grace/DeletionQueue.hpp>
#include <Grace/Buffer.hpp>
#include <Grace/Image.hpp>
#include <Grace/Sampler.hpp>
#include <Grace/PipelineGroup.hpp>
#include <Grace/Fence.hpp>
#include <Grace/Semaphore.hpp>
#include <Grace/HandleTypes.hpp>
#include <Grace/Macros.hpp>

namespace Grace
{

class Device;

template <typename Res>
struct RegistryEntry
{
    template <typename... Args>
    RegistryEntry(const uint32_t Validator, Args&&... args)
        : validator(Validator), resource(std::forward<Args>(args)...)
    {
    }

    Res resource = {};
    uint32_t validator = 0U;
};

template <typename Res>
class Registry
{
public:
    std::vector<RegistryEntry<Res>>& GetAll()
    {
        return m_Registry;
    }

    template <typename... Args>
    Handle<Res> Register(Args&&... args)
    {
        // Use free slots if any available
        if (!m_FreeSlots.empty())
        {
            Handle<Res> newHandle = m_FreeSlots.front();
            m_FreeSlots.pop();
            m_Registry[newHandle.handle].validator = newHandle.validator;
            m_Registry[newHandle.handle].resource = Res(std::forward<Args>(args)...);
            return newHandle;
        }

        uint32_t newValidator = m_Validator++;
        m_Registry.emplace_back(newValidator, std::forward<Args>(args)...);

        const uint32_t index = static_cast<uint32_t>(m_Registry.size() - 1);
        return Handle<Res>(index, newValidator);
    }

    Res& Get(const Handle<Res>& resourceHandle)
    {
        assert(resourceHandle.HasValidHandle() && "Handle is invalid.");
        const bool handleInRange = resourceHandle.handle < m_Registry.size();
        assert(handleInRange && "Handle is not in range.");
        const bool isValidSlot = m_Registry[resourceHandle.handle].validator == resourceHandle.validator;
        assert(isValidSlot && "Handle validator does not match validator of the slot it's in.");

        return m_Registry[resourceHandle.handle].resource;
    }

    void Free(Handle<Res>& resourceHandle, bool deferred)
    {
        assert(resourceHandle.HasValidHandle() && "Handle is invalid.");
        const bool handleInRange = resourceHandle.handle < m_Registry.size();
        assert(handleInRange && "Handle is not in range.");
        const bool isValidSlot = m_Registry[resourceHandle.handle].validator == resourceHandle.validator;
        assert(isValidSlot && "Handle validator does not match validator of the slot it's in.");

        m_Registry[resourceHandle.handle].resource = Res();
        m_Registry[resourceHandle.handle].validator = INVALID_VALIDATOR;

        // Slot is freed up and can be reused for the next resource created
        m_FreeSlots.emplace(resourceHandle.handle, ++m_Validator);

        // Invalidate resourceHandle
        if (!deferred)
        {
            resourceHandle.handle = INVALID_HANDLE;
            resourceHandle.validator = INVALID_VALIDATOR;
        }
    }

private:
    std::vector<RegistryEntry<Res>> m_Registry = {};
    std::queue<Handle<Res>> m_FreeSlots = {};
    uint32_t m_Validator = 0;
};

/// Handles all graphics resources through registry containers.
///
/// Should call `FreeAllResources` on shutdown to automate cleanup.
///
/// Each `RegistryEntry` of the registry containers has an assigned `validator` which is used
/// to validate the slot against the `ResourceHandle` that points to it. If the validation
/// fails, the slot and handle are incorrectly paired so the operation, free or fetch, terminates.
class ResourceManager
{
public:
    explicit ResourceManager(uint32_t framesInFlight);

    void FlushDeletionQueue(uint32_t frameIndex);

    template <typename Res, typename... Args>
    GRACE_NODISCARD Handle<Res> Create(Args&&... args)
    {
        return ResourceRegistry<Res>().Register(std::forward<Args>(args)...);
    }

    template <typename Res>
    GRACE_NODISCARD Res& Get(Handle<Res> handle)
    {
        return ResourceRegistry<Res>().Get(handle);
    }

    template <typename Res>
    void Free(Handle<Res>& handle, uint32_t frameIndex = UINT32_MAX)
    {
        if (frameIndex != UINT32_MAX)
        {
            m_DeletionQueue->PushDeleter(
                [&]()
                {
                    ResourceRegistry<Res>().Free(handle, true);
                },
                frameIndex);
            return;
        }
        ResourceRegistry<Res>().Free(handle, false);
    }

    /// Fetches ALL Images found in the Image's Registry
    GRACE_NODISCARD std::vector<RegistryEntry<Image>>& GetAllImages();

private:
    std::unique_ptr<DeletionQueue> m_DeletionQueue = nullptr;

    template <typename Res>
    auto& ResourceRegistry();

    GRACE_DEFINE_RESOURCE_REGISTRY(Image, m_ImagesRegistry);
    GRACE_DEFINE_RESOURCE_REGISTRY(Buffer, m_BuffersRegistry);
    GRACE_DEFINE_RESOURCE_REGISTRY(Sampler, m_SamplersRegistry);
    GRACE_DEFINE_RESOURCE_REGISTRY(Pipeline, m_PipelinesRegistry);
    GRACE_DEFINE_RESOURCE_REGISTRY(PipelineLayout, m_PipelineLayoutsRegistry);
    GRACE_DEFINE_RESOURCE_REGISTRY(Fence, m_FencesRegistry);
    GRACE_DEFINE_RESOURCE_REGISTRY(BinarySemaphore, m_BinarySemaphoresRegistry);
    GRACE_DEFINE_RESOURCE_REGISTRY(TimelineSemaphore, m_TimelineSemaphoresRegistry);
};

} // namespace Grace
