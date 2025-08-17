#include "GpuResourceTable.hpp"

#include <Grace/DebugReporter.hpp>
#include <Grace/Context.hpp>
#include <Grace/Buffer.hpp>
#include <Grace/Image.hpp>
#include <Grace/Sampler.hpp>

#include <cassert>

namespace Grace
{

void SlotPool::SetPoolSize(const uint32_t maxSize)
{
    m_MaxSlots = maxSize;
}

void SlotPool::AppendFreeSlot(const uint32_t slot)
{
    m_FreeSlots.emplace_front(slot);
}

uint32_t SlotPool::FindAvailableSlot()
{
    assert(m_CurrentSlot < m_MaxSlots);

    if (!m_FreeSlots.empty())
    {
        uint32_t slotIndex = m_FreeSlots.back();
        m_FreeSlots.pop_back();
        return slotIndex;
    }

    return m_CurrentSlot++;
}

GpuResourceTable::~GpuResourceTable()
{
    vkDestroyDescriptorPool(m_Device->GetVkHandle(), bindlessDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_Device->GetVkHandle(), bindlessDescriptorSetLayout, nullptr);
}

GpuResourceTable::GpuResourceTable(Device* pDevice, uint32_t maxImages, uint32_t maxSamplers, uint32_t maxBuffers)
    : m_Device(pDevice)
{
    assert(maxImages > 0);
    assert(maxBuffers > 0);
    assert(maxSamplers > 0);

    m_StorageImageSlots.SetPoolSize(maxImages);
    m_SampledImageSlots.SetPoolSize(maxImages);
    m_UniformBufferSlots.SetPoolSize(maxBuffers);
    m_SamplerSlots.SetPoolSize(maxSamplers);

    m_Writer.SetDevice(m_Device->GetVkHandle());

    // Pool Sizes, identical order to bindings
    std::vector<VkDescriptorPoolSize> poolSizes = { { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxImages },
                                                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxImages },
                                                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxImages },
                                                    { VK_DESCRIPTOR_TYPE_SAMPLER, maxSamplers },
                                                    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxBuffers } };

    // Create global descriptor pool
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    DebugReporter::Check(vkCreateDescriptorPool(m_Device->GetVkHandle(), &poolInfo, nullptr, &bindlessDescriptorPool));
    AssignDebugName(m_Device->GetVkHandle(), bindlessDescriptorPool, "Grace::Bindless::DescriptorPool");

    // Enable Update-After-Bind and Partially-Bound
    // clang-format off
    std::vector<VkDescriptorBindingFlags> bindingFlags = {
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {};
    bindingFlagsInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.pNext         = nullptr;
    bindingFlagsInfo.bindingCount  = static_cast<uint32_t>(bindingFlags.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags.data();

    // Create global descriptor set layout
    DescriptorLayoutBuilder builder(m_Device->GetVkHandle());
    builder.AddBinding(static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::StorageImage), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxImages);
    builder.AddBinding(static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::SampledImage), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxImages);
    builder.AddBinding(static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::CombinedImageSampler), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxImages);
    builder.AddBinding(static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::Sampler), VK_DESCRIPTOR_TYPE_SAMPLER, maxSamplers);
    builder.AddBinding(static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::UniformBuffer), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    bindlessDescriptorSetLayout = builder.Build(VK_SHADER_STAGE_ALL, &bindingFlagsInfo, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);
    AssignDebugName(m_Device->GetVkHandle(), bindlessDescriptorSetLayout, "Grace::Bindless::DescriptorSetLayout");
    // clang-format on

    // Allocate global descriptor set
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = bindlessDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &bindlessDescriptorSetLayout;

    DebugReporter::Check(vkAllocateDescriptorSets(m_Device->GetVkHandle(), &allocInfo, &bindlessDescriptorSet));
    AssignDebugName(m_Device->GetVkHandle(), bindlessDescriptorSet, "Grace::Bindless::DescriptorSet");

    // Create global pipeline layout
    VkPushConstantRange pushConstants = {};
    pushConstants.offset = 0;
    pushConstants.size = 128;
    pushConstants.stageFlags = VK_SHADER_STAGE_ALL;

    bindlessPipelineLayout = m_Device->CreatePipelineLayout({
        .name = "Grace::Bindless::PipelineLayout",
        .flags = 0,
        .setLayouts = { bindlessDescriptorSetLayout },
        .pushConstantRanges = { pushConstants },
    });
}

void GpuResourceTable::SubmitImage(Image& image)
{
    assert(!image.IsNull());

    // Query image usage flags
    VkImageUsageFlags usageFlags = image.GetUsageFlags();

    uint32_t sampledImgId = 0;
    uint32_t storageImgId = 0;

    // Add to an available slot in Registry
    if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        storageImgId = m_StorageImageSlots.FindAvailableSlot();
        m_Writer.WriteImageBindless(storageImgId,
                                    static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::StorageImage),
                                    image.GetDefaultView().GetVkHandle(),
                                    nullptr,
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }
    if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        sampledImgId = m_SampledImageSlots.FindAvailableSlot();
        m_Writer.WriteImageBindless(sampledImgId,
                                    static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::SampledImage),
                                    image.GetDefaultView().GetVkHandle(),
                                    nullptr,
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    }

    // Set resource id
    image.SetStorageImgId(storageImgId);
    image.SetSampledImgId(sampledImgId);
}

void GpuResourceTable::FreeImage(const Image& image)
{
    if (image.GetUsageFlags() & VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        m_SampledImageSlots.AppendFreeSlot(image.GetSampledImgId());
    }

    if (image.GetUsageFlags() & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        m_StorageImageSlots.AppendFreeSlot(image.GetStorageImgId());
    }
}

void GpuResourceTable::SubmitImageView(ImageView& view)
{
    uint32_t storageImgId = 0;

    if (view.GetUsageFlags() & VK_IMAGE_USAGE_STORAGE_BIT)
    {
        storageImgId = m_StorageImageSlots.FindAvailableSlot();
        m_Writer.WriteImageBindless(storageImgId,
                                    static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::StorageImage),
                                    view.GetVkHandle(),
                                    nullptr,
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }

    // Set resource id
    view.SetStorageImgId(storageImgId);
}

void GpuResourceTable::SubmitSampler(Sampler& sampler)
{
    uint32_t samplerId = m_SamplerSlots.FindAvailableSlot();
    m_Writer.WriteSamplerBindless(
        samplerId, static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::Sampler), sampler);

    sampler.SetSamplerId(samplerId);
}

void GpuResourceTable::FreeSampler(const Sampler& sampler)
{
    m_SamplerSlots.AppendFreeSlot(sampler.GetSamplerId());
}

void GpuResourceTable::SubmitBuffer(const Buffer& buffer)
{
    VmaAllocationInfo2 allocInfo = buffer.GetAllocationInfo();

    m_Writer.WriteBuffer(static_cast<uint32_t>(Bindless::DescriptorTypeBindingIndex::UniformBuffer),
                         buffer.GetVkHandle(),
                         allocInfo.allocationInfo.size,
                         0,
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void GpuResourceTable::UpdateTable()
{
    m_Writer.UpdateSet(bindlessDescriptorSet);
}

} // namespace Grace
