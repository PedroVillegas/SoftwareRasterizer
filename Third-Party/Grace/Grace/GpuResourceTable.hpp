#pragma once

#include <Grace/DescriptorGroup.hpp>
#include <Grace/Macros.hpp>
#include <Grace/HandleTypes.hpp>

namespace Grace
{

class ImageView;
class Image;
class Buffer;
class Sampler;
class Device;

namespace Bindless
{

enum class DescriptorTypeBindingIndex : uint32_t
{
    StorageImage = 0U,
    SampledImage = 1U,
    CombinedImageSampler = 2U,
    Sampler = 3U,
    UniformBuffer = 4U,
};

}; // namespace Bindless

class SlotPool
{
public:
    /// Set the max size of slots
    void SetPoolSize(const uint32_t maxSize);

    /// Pushes the freed slot index to the front of the Free Slots queue
    void AppendFreeSlot(const uint32_t slot);

    /// @returns Index to first unoccupied slot.
    GRACE_NODISCARD uint32_t FindAvailableSlot();

private:
    uint32_t m_MaxSlots = std::numeric_limits<uint16_t>::max();
    uint32_t m_CurrentSlot = 0U;
    std::deque<uint32_t> m_FreeSlots = {};
};

/// Handles resource array ids.
///
/// When submitting a valid resource, it will be allocated a slot in the GPU resource array
/// which is then set for the resource.
class GpuResourceTable
{
public:
    ~GpuResourceTable();
    GpuResourceTable() = default;
    GpuResourceTable(Device* pDevice, uint32_t maxImages, uint32_t maxSamplers, uint32_t maxBuffers);

    GpuResourceTable(const GpuResourceTable&) = delete;
    GpuResourceTable& operator=(const GpuResourceTable&) = delete;

    GpuResourceTable(GpuResourceTable&&) noexcept = delete;
    GpuResourceTable& operator=(GpuResourceTable&&) noexcept = delete;

    /// Finds and sets an available resource id slot based on images usage flags.
    void SubmitImage(Image& image);

    /// Frees and returns image's sampled and/or storage id back to the Images Slot Pool.
    void FreeImage(const Image& image);

    /// Finds and sets an available resource id slot based on images usage flags.
    void SubmitImageView(ImageView& view);

    /// Finds and sets an available resource id slot for a given sampler.
    void SubmitSampler(Sampler& sampler);

    /// Frees and returns sampler's resource id back to the Samplers Slot Pool.
    void FreeSampler(const Sampler& sampler);

    /// Finds and sets an available resource id slot for a given buffer.
    void SubmitBuffer(const Buffer& buffer);

    void UpdateTable();

    VkDescriptorPool bindlessDescriptorPool = {};
    VkDescriptorSetLayout bindlessDescriptorSetLayout = {};
    VkDescriptorSet bindlessDescriptorSet = {};
    PipelineLayoutHandle bindlessPipelineLayout = {};

private:
    Device* m_Device = nullptr;

    /// The one and only `DescriptorWriter` to batch update the Sole Descriptor Set.
    DescriptorWriter m_Writer = {};

    SlotPool m_StorageImageSlots = {};
    SlotPool m_SampledImageSlots = {};
    SlotPool m_SamplerSlots = {};
    SlotPool m_UniformBufferSlots = {};
};

} // namespace Grace
