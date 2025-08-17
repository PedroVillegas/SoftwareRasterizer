#pragma once

#include <vector>
#include <span>
#include <deque>

#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

class Sampler;
class Image;
class Buffer;

// ----------------------------------------------------------------------------------
//                              DESCRIPTOR ALLOCATOR
// ----------------------------------------------------------------------------------

/// @brief Contains `VkDescriptorType` and a `ratio`
/// to multiply the maxSets by.
struct PoolSizeRatio
{
    VkDescriptorType type = {};
    float ratio = {};
};

/// @brief A growable abstraction of `VkDescriptorPool`s.
///
/// Handles a bunch of `VkDescriptorPool`s to allocate from stored in
/// `fullPools` and `readyPools` containers.
///
/// Begin with a few pools in `readyPools` pile and `fullPools` empty.
/// When a pool is full, and thus fails to allocate, it is moved into `fullPools`
/// and a new pool is created and added to `readyPools`.
class DescriptorAllocator
{
public:
    DescriptorAllocator(VkDevice device) : m_Device(device) {};
    DescriptorAllocator() = default;

    void SetDevice(VkDevice device);

    /// @brief Initialises `readyPools` and `fullPools`.
    ///
    /// @param pJanitor
    /// @param maxSets Maximum number of descriptor sets per pool.
    /// @param poolRatios
    void Initialise(uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);

    /// @brief Clears `readyPools` and `fullPools` without deleting the `VkDescriptorPool`s.
    void ClearPools();

    /// @brief Free all `VkDescriptorPool`s
    void CleanupPools();

    /// @brief Searches `readyPools` for a free pool to allocate a `VkDescriptorSet` from.
    ///
    /// @param layout `VkDescriptorSetLayout` of the descriptor set to be allocated.
    /// @param pNext Optional pointer to next struct for `VkDescriptorSetAllocateInfo`.
    ///
    /// @returns Allocated `VkDescriptorSet` corresponding to given layout.
    GRACE_NODISCARD VkDescriptorSet Allocate(VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
    GRACE_NODISCARD VkDescriptorPool GetPool();
    GRACE_NODISCARD VkDescriptorPool CreatePool(uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    VkDevice m_Device = {};

    std::vector<PoolSizeRatio> m_Ratios = {};
    std::vector<VkDescriptorPool> m_FullPools = {};
    std::vector<VkDescriptorPool> m_ReadyPools = {};
    uint32_t m_SetsPerPool = {};
};

// ----------------------------------------------------------------------------------
//                              DESCRIPTOR WRITER
// ----------------------------------------------------------------------------------

/// @brief Abstraction for writing images or buffers to descriptor sets.
class DescriptorWriter
{
public:
    DescriptorWriter(VkDevice device) : m_Device(device) {};
    DescriptorWriter() = default;

    void SetDevice(VkDevice device);

    /// @brief Clears all currently queued writes.
    void Clear();

    /// @brief Updates given `VkDescriptorSet` with the writes corresponding to the bindings.
    ///
    /// @param set Descriptor set to be updated.
    void UpdateSet(VkDescriptorSet set);

    void WriteSamplerBindless(const uint32_t resourceId, int binding, const Sampler& sampler);

    void WriteImageBindless(uint32_t resourceId,
                            int binding,
                            const Image& image,
                            VkSampler sampler,
                            VkImageLayout layout,
                            VkDescriptorType type);

    void WriteImageBindless(uint32_t resourceId,
                            int binding,
                            VkImageView view,
                            VkSampler sampler,
                            VkImageLayout layout,
                            VkDescriptorType type);

    void WriteBufferBindless(
        uint32_t resourceId, int binding, const Buffer& buffer, size_t size, size_t offset, VkDescriptorType type);

    /// @brief Writes image.
    ///
    /// @param binding Corresponding binding in shader.
    /// @param image `Image` being written.
    /// @param sampler `VkSampler` used to sample image in shader.
    /// @param layout `VkImageLayout` of image being written.
    ///               `VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL` for combined image samplers.
    ///               `VK_IMAGE_LAYOUT_GENERAL` for storage images.
    /// @param type `VkDescriptorType` of image e.g. `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`.
    void WriteImage(int binding, const Image& image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);

    /// @brief Writes image.
    ///
    /// @param binding Corresponding binding in shader.
    /// @param image `VkImageView` of `Image` being written.
    /// @param sampler `VkSampler` used to sample image in shader.
    /// @param layout `VkImageLayout` of image being written.
    ///               `VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL` for combined image samplers.
    ///               `VK_IMAGE_LAYOUT_GENERAL` for storage images.
    /// @param type `VkDescriptorType` of image e.g. `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`.
    void WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);

    /// @brief Writes buffer.
    ///
    /// @param binding Corresponding binding in shader.
    /// @param buffer `Buffer` being written.
    /// @param size Size of buffer, in bytes.
    /// @param offset Offset into buffer, in bytes.
    /// @param type `VkDescriptorType` of buffer e.g. `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`.
    void WriteBuffer(int binding, const Buffer& buffer, size_t size, size_t offset, VkDescriptorType type);

    /// @brief Writes buffer.
    ///
    /// @param binding Corresponding binding in shader.
    /// @param buffer `VkBuffer` of `Buffer` being written.
    /// @param size Size of buffer, in bytes.
    /// @param offset Offset into buffer, in bytes.
    /// @param type `VkDescriptorType` of buffer e.g. `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`.
    void WriteBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

private:
    VkDevice m_Device = {};

    std::deque<VkDescriptorImageInfo> imageInfos = {};
    std::deque<VkDescriptorBufferInfo> bufferInfos = {};
    std::vector<VkWriteDescriptorSet> writes = {};
};

// ----------------------------------------------------------------------------------
//                          DESCRIPTOR LAYOUT BUILDER
// ----------------------------------------------------------------------------------

/// A builder class that stores bindings as `VkDescriptorSetLayoutBinding`
/// in a container and later bundles all bindings into a single `VkDescriptorSetLayout`.
///
/// Adding a binding requires a call to `AddBinding`.
///
/// Building the descriptor set layout requires a call to `Build`.
///
/// Bindings can be cleared at any time with a call to `Clear`.
class DescriptorLayoutBuilder
{
public:
    DescriptorLayoutBuilder(VkDevice device) : m_Device(device) {};
    DescriptorLayoutBuilder() = default;

    /// @brief Adds a binding to the descriptor layout.
    ///
    /// @param binding Binding location as found in the shader.
    /// @param type Resource type found in the `binding`.
    /// @param count Number of descriptors allocated for this binding. `Default value = 1`.
    void AddBinding(uint32_t binding, VkDescriptorType type, uint32_t count = 1);

    /// @brief Builds a `VkDescriptorSetLayout`.
    ///
    /// @param shaderStages Shader stages where the descriptor can be found.
    /// @param pNext Optional, used for extensions.
    /// @param flags Flags for layout creation e.g. Update-After-Bind.
    ///
    /// @returns `VkDescriptorSetLayout` of all bindings added thus far.
    GRACE_NODISCARD VkDescriptorSetLayout
    Build(VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

    /// @brief Clear all bindings that have been added.
    void Clear();

private:
    VkDevice m_Device = {};

    std::vector<VkDescriptorSetLayoutBinding> m_Bindings = {};
};

} // namespace Grace
