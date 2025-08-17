#include "Image.hpp"

#include "HelperFunctions.hpp"

#include <Grace/DebugReporter.hpp>
#include <Grace/Context.hpp>

#include <cassert>
#include <cmath>

namespace Grace
{

ImageView::~ImageView()
{
    if (m_Device != nullptr)
    {
        vkDestroyImageView(m_Device->GetVkHandle(), m_View, nullptr);
    }
}

ImageView::ImageView(Device* pDevice, const ImageViewDesc& desc) : m_Device(pDevice)
{
    assert(!m_Device->IsNull());

    m_ParentImage = desc.image;

    const VkImageAspectFlags aspectMask = DetermineImageAspectFlagsFromFormat(m_ParentImage->GetFormat());

    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = nullptr;
    info.viewType = m_ParentImage->GetExtent3D().height > 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_1D;
    info.viewType = m_ParentImage->GetExtent3D().depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
    info.image = m_ParentImage->GetImage();
    info.format = m_ParentImage->GetFormat();
    info.subresourceRange.baseMipLevel = desc.mipLevel;
    info.subresourceRange.levelCount = desc.levelCount;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.aspectMask = aspectMask;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    DebugReporter::Check(vkCreateImageView(m_Device->GetVkHandle(), &info, nullptr, &m_View));
    AssignDebugName<VkImageView>(m_Device->GetVkHandle(), m_View, desc.name);
}

ImageView::ImageView(ImageView&& other) noexcept
    : m_Device(other.m_Device), m_ParentImage(other.m_ParentImage), m_View(other.m_View)
{
    other.m_View = nullptr;
}

ImageView& ImageView::operator=(ImageView&& other) noexcept
{
    if (m_Device != nullptr)
    {
        vkDestroyImageView(m_Device->GetVkHandle(), m_View, nullptr);
    }

    m_Device = other.m_Device;
    m_ParentImage = other.m_ParentImage;
    m_View = other.m_View;
    other.m_Device = nullptr;
    other.m_ParentImage = nullptr;
    other.m_View = nullptr;

    return *this;
}

bool ImageView::IsNull() const
{
    return m_View == nullptr;
}

VkImageView ImageView::GetVkHandle() const
{
    return m_View;
}

void ImageView::MakeNull()
{
    m_View = nullptr;
}

uint32_t ImageView::GetStorageImgId() const
{
    return m_StorageImgId;
}

void ImageView::SetStorageImgId(uint32_t storageImgId)
{
    m_StorageImgId = storageImgId;
}

VkImageUsageFlags ImageView::GetUsageFlags() const
{
    return m_ParentImage->GetUsageFlags();
}

Image::~Image()
{
    if (m_Device != nullptr && !m_IsSwapchainImage)
    {
        vmaDestroyImage(m_Device->GetVmaHandle(), m_Image, m_Allocation);
    }
}

Image::Image(Device* pDevice, const ImageDesc& desc)
    : m_Device(pDevice), m_Format(desc.format), m_Extent(desc.dimensions), m_UsageFlags(desc.usage)
{
    assert(!pDevice->IsNull());
    assert(desc.usage != 0);
    assert(desc.dimensions.width > 0);

    const uint32_t mipLevels = desc.mipmapped ? GetMaxMipLevels() : 1;
    const VkImageAspectFlags aspectMask = DetermineImageAspectFlagsFromFormat(desc.format);

    VkImageCreateInfo imgcinfo = {};
    imgcinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgcinfo.pNext = nullptr;
    imgcinfo.flags = 0;
    imgcinfo.imageType = desc.dimensions.height > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D;
    imgcinfo.imageType = desc.dimensions.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    imgcinfo.format = m_Format;
    imgcinfo.extent = m_Extent;
    imgcinfo.mipLevels = mipLevels;
    imgcinfo.arrayLayers = 1;
    imgcinfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgcinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgcinfo.usage = desc.usage;
    if (desc.mipmapped)
        imgcinfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imgcinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    DebugReporter::Check(
        vmaCreateImage(m_Device->GetVmaHandle(), &imgcinfo, &allocInfo, &m_Image, &m_Allocation, nullptr));
    AssignDebugName<VkImage>(m_Device->GetVkHandle(), m_Image, desc.name);

    m_DefaultView = ImageView(m_Device,
                              {
                                  .name = desc.name,
                                  .image = this,
                                  .mipLevel = 0,
                                  .levelCount = mipLevels,
                              });

    if (desc.data != nullptr || desc.access != AccessType::None)
    {
        BufferHandle stagingBuffer;

        const CommandBuffer& cmd = m_Device->BeginSingleTimeCommands();
        const std::string debugLabel = desc.name + std::string(" | Setup");
        cmd.BeginDebugLabel(debugLabel.c_str(), { 1.0F, 1.0F, 1.0F, 1.0F });

        if (desc.data != nullptr)
        {
            stagingBuffer = pDevice->CreateBuffer({
                .name = "Staging Buffer",
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                .size = desc.size,
                .data = nullptr,
            });

            m_Device->CopyMemoryToHostVisibleBuffer(stagingBuffer, 0, desc.data, desc.size);

            VkImageMemoryBarrier2 layoutTransition = {};
            layoutTransition.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            layoutTransition.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            layoutTransition.srcAccessMask = VK_ACCESS_2_NONE;
            layoutTransition.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
            layoutTransition.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            layoutTransition.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            layoutTransition.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            layoutTransition.image = m_Image;
            layoutTransition.subresourceRange.aspectMask = aspectMask;
            layoutTransition.subresourceRange.baseMipLevel = 0;
            layoutTransition.subresourceRange.levelCount = mipLevels;
            layoutTransition.subresourceRange.baseArrayLayer = 0;
            layoutTransition.subresourceRange.layerCount = 1;

            VkDependencyInfo depInfo = {};
            depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &layoutTransition;

            vkCmdPipelineBarrier2(cmd.GetVkCommandBuffer(), &depInfo);

            // Copy image data to staging buffer, then copy staging buffer to image; image stays gpu visible only
            VkBufferImageCopy2 copyRegion = {};
            copyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
            copyRegion.pNext = nullptr;
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = aspectMask;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset = { .x = 0, .y = 0, .z = 0 };
            copyRegion.imageExtent = { .width = desc.dimensions.width,
                                       .height = desc.dimensions.height,
                                       .depth = desc.dimensions.depth };

            VkCopyBufferToImageInfo2 copyInfo = {};
            copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
            copyInfo.pNext = nullptr;
            copyInfo.srcBuffer = m_Device->GetBuffer(stagingBuffer).GetVkHandle();
            copyInfo.dstImage = m_Image;
            copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
            copyInfo.regionCount = 1;
            copyInfo.pRegions = &copyRegion;

            vkCmdCopyBufferToImage2(cmd.GetVkCommandBuffer(), &copyInfo);

            if (desc.mipmapped)
            {
                {
                    VkMemoryBarrier2 memBarrier = {};
                    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                    memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
                    memBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                    memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
                    memBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;

                    VkDependencyInfo depInfo = {};
                    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                    depInfo.memoryBarrierCount = 1;
                    depInfo.pMemoryBarriers = &memBarrier;

                    vkCmdPipelineBarrier2(cmd.GetVkCommandBuffer(), &depInfo);
                }

                VkExtent2D imageSize = { desc.dimensions.width, desc.dimensions.height };
                for (uint32_t mip = 0; mip < mipLevels; mip++)
                {
                    VkExtent2D halfSize = imageSize;
                    halfSize.width /= 2;
                    halfSize.height /= 2;

                    if (mip < mipLevels - 1)
                    {
                        VkImageBlit2 blitRegion = {};
                        blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
                        blitRegion.pNext = nullptr;
                        blitRegion.srcOffsets[1].x = imageSize.width;
                        blitRegion.srcOffsets[1].y = imageSize.height;
                        blitRegion.srcOffsets[1].z = 1;
                        blitRegion.dstOffsets[1].x = halfSize.width;
                        blitRegion.dstOffsets[1].y = halfSize.height;
                        blitRegion.dstOffsets[1].z = 1;
                        blitRegion.srcSubresource.aspectMask = aspectMask;
                        blitRegion.srcSubresource.baseArrayLayer = 0;
                        blitRegion.srcSubresource.layerCount = 1;
                        blitRegion.srcSubresource.mipLevel = mip;
                        blitRegion.dstSubresource.aspectMask = aspectMask;
                        blitRegion.dstSubresource.baseArrayLayer = 0;
                        blitRegion.dstSubresource.layerCount = 1;
                        blitRegion.dstSubresource.mipLevel = mip + 1;

                        VkBlitImageInfo2 blitInfo = {};
                        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
                        blitInfo.pNext = nullptr;
                        blitInfo.srcImage = m_Image;
                        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL;
                        blitInfo.dstImage = m_Image;
                        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL;
                        blitInfo.filter = VK_FILTER_LINEAR;
                        blitInfo.regionCount = 1;
                        blitInfo.pRegions = &blitRegion;

                        cmd.BlitImage(blitInfo);
                        imageSize = halfSize;
                    }

                    {
                        VkMemoryBarrier2 memBarrier = {};
                        memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
                        memBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
                        memBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                        memBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
                        memBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

                        VkDependencyInfo depInfo = {};
                        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                        depInfo.memoryBarrierCount = 1;
                        depInfo.pMemoryBarriers = &memBarrier;

                        vkCmdPipelineBarrier2(cmd.GetVkCommandBuffer(), &depInfo);
                    }
                }
            }
        }

        if (desc.access != AccessType::None)
        {
            VkImageMemoryBarrier2 layoutTransition = {};
            layoutTransition.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            layoutTransition.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            layoutTransition.srcAccessMask = VK_ACCESS_2_NONE;
            layoutTransition.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
            layoutTransition.dstAccessMask = VK_ACCESS_2_NONE;
            layoutTransition.oldLayout = desc.data == nullptr ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
            layoutTransition.newLayout = AccessTypeMap[static_cast<uint32_t>(desc.access)].imageLayout;
            layoutTransition.image = m_Image;
            layoutTransition.subresourceRange.aspectMask = aspectMask;
            layoutTransition.subresourceRange.baseMipLevel = 0;
            layoutTransition.subresourceRange.levelCount = mipLevels;
            layoutTransition.subresourceRange.baseArrayLayer = 0;
            layoutTransition.subresourceRange.layerCount = 1;

            VkDependencyInfo depInfo = {};
            depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            depInfo.imageMemoryBarrierCount = 1;
            depInfo.pImageMemoryBarriers = &layoutTransition;

            vkCmdPipelineBarrier2(cmd.GetVkCommandBuffer(), &depInfo);
        }

        cmd.EndDebugLabel();
        m_Device->EndAndSubmitSingleTimeCommands();

        if (desc.data != nullptr)
        {
            m_Device->FreeBuffer(stagingBuffer);
        }
    }
}

Image::Image(Device* pDevice, VkImage image, const ImageDesc& desc)
    : m_Device(pDevice), m_Image(image), m_Format(desc.format), m_Extent(desc.dimensions), m_UsageFlags(desc.usage),
      m_IsSwapchainImage(true)
{
    assert(!pDevice->IsNull());
    assert(image != nullptr);
    assert(desc.usage != 0);

    m_DefaultView = ImageView(pDevice,
                              {
                                  .name = desc.name,
                                  .image = this,
                                  .mipLevel = 0,
                                  .levelCount = 1,
                              });

    AssignDebugName<VkImage>(m_Device->GetVkHandle(), m_Image, desc.name);
}

Image::Image(Image&& other) noexcept
    : m_Device(other.m_Device), m_DefaultView(std::move(other.m_DefaultView)), m_Image(other.m_Image),
      m_Allocation(other.m_Allocation), m_Extent(other.m_Extent), m_Format(other.m_Format),
      m_UsageFlags(other.m_UsageFlags), m_IsSwapchainImage(other.m_IsSwapchainImage)
{
    other.m_Device = VK_NULL_HANDLE;
    other.m_Image = VK_NULL_HANDLE;
    other.m_Allocation = VK_NULL_HANDLE;
}

Image& Image::operator=(Image&& other) noexcept
{
    if (m_Device != nullptr && !m_IsSwapchainImage)
    {
        vmaDestroyImage(m_Device->GetVmaHandle(), m_Image, m_Allocation);
    }

    m_Device = other.m_Device;
    m_Image = other.m_Image;
    m_Allocation = other.m_Allocation;
    m_Format = other.m_Format;
    m_Extent = other.m_Extent;
    m_UsageFlags = other.m_UsageFlags;
    m_IsSwapchainImage = other.m_IsSwapchainImage;
    m_DefaultView = std::move(other.m_DefaultView);
    other.m_Device = VK_NULL_HANDLE;
    other.m_Image = VK_NULL_HANDLE;
    other.m_Allocation = VK_NULL_HANDLE;

    return *this;
}

void Image::SetStorageImgId(uint32_t id)
{
    m_StorageImgId = id;
}

void Image::SetSampledImgId(uint32_t id)
{
    m_SampledImgId = id;
}

uint32_t Image::GetStorageImgId() const
{
    assert(m_UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT);
    return m_StorageImgId;
}

uint32_t Image::GetSampledImgId() const
{
    assert(m_UsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT);
    return m_SampledImgId;
}

bool Image::IsNull() const
{
    const bool needsAllocationCheck = m_IsSwapchainImage ? false : m_Allocation == nullptr;
    return m_Image == nullptr || m_DefaultView.GetVkHandle() == nullptr || needsAllocationCheck || m_UsageFlags == 0;
}

const VkImage& Image::GetImage() const
{
    return m_Image;
}

const ImageView& Image::GetDefaultView() const
{
    return m_DefaultView;
}

const VkFormat& Image::GetFormat() const
{
    return m_Format;
}

VkExtent2D Image::GetExtent2D() const
{
    return { m_Extent.width, m_Extent.height };
}

const VkExtent3D& Image::GetExtent3D() const
{
    return m_Extent;
}

uint32_t Image::GetWidth() const
{
    return m_Extent.width;
}

uint32_t Image::GetHeight() const
{
    return m_Extent.height;
}

uint32_t Image::GetDepth() const
{
    return m_Extent.depth;
}

uint32_t Image::GetMaxMipLevels() const
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(m_Extent.width, m_Extent.height)))) + 1;
}

VkImageUsageFlags Image::GetUsageFlags() const
{
    return m_UsageFlags;
}

const VmaAllocation& Image::GetAllocation() const
{
    return m_Allocation;
}

VmaAllocationInfo2 Image::GetAllocationInfo() const
{
    VmaAllocationInfo2 info = {};
    vmaGetAllocationInfo2(m_Device->GetVmaHandle(), m_Allocation, &info);
    return info;
}

} // namespace Grace
