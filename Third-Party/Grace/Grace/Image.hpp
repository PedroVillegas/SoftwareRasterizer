#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>
#include <Grace/SyncGroup.hpp>

namespace Grace
{

class Device;
class Image;

struct GRACE_EXPORT ImageViewDesc
{
    const char* name;
    Image* image;
    uint32_t mipLevel;
    uint32_t levelCount;
};

class GRACE_EXPORT ImageView
{
public:
    ~ImageView();
    ImageView() = default;
    ImageView(Device* pDevice, const ImageViewDesc& desc);

    // Copy constructions/assignments are prohibited to stop destructor trying to
    // destroy the same VkImageView handle more than once
    ImageView(const ImageView&) = delete;
    ImageView& operator=(const ImageView&) = delete;

    ImageView(ImageView&& other) noexcept;
    ImageView& operator=(ImageView&& other) noexcept;

    GRACE_NODISCARD bool IsNull() const;

    GRACE_NODISCARD VkImageView GetVkHandle() const;

    void MakeNull();

    GRACE_NODISCARD uint32_t GetStorageImgId() const;

    void SetStorageImgId(uint32_t storageImgId);

    GRACE_NODISCARD VkImageUsageFlags GetUsageFlags() const;

private:
    Device* m_Device = nullptr;
    Image* m_ParentImage = nullptr;
    VkImageView m_View = nullptr;
    uint32_t m_StorageImgId = 0;
};

/// Description used to create an Image object
struct GRACE_EXPORT ImageDesc
{
    /// Name used to identify the image, e.g. in validation errors
    const char* name;
    /// Specifies the image's dimensions
    VkExtent3D dimensions;
    /// Specifies the image's format
    VkFormat format;
    /// Specifies how the image is allowed to be used
    VkImageUsageFlags usage;
    /// Specifies what access type the image should be initialised for upon creation
    AccessType access = AccessType::None;
    /// Size of data in bytes
    size_t size = 0;
    /// Pointer to data used to fill the image with upon creation
    const void* data = nullptr;
    /// Specifies whether mipmaps should be generated
    bool mipmapped = false;
};

class GRACE_EXPORT Image
{
public:
    ~Image();
    Image() = default;
    Image(Device* pDevice, const ImageDesc& desc);
    Image(Device* pDevice, VkImage image, const ImageDesc& desc); // Specifically for swapchain images

    // Copy constructions/assignments are prohibited to stop destructor trying to
    // destroy the same VkImage handle more than once
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    /// Sets index to resource in bindless array of Storage Images for access on GPU.
    void SetStorageImgId(uint32_t id);

    /// Sets index to resource in bindless array of Sampled Images for access on GPU.
    void SetSampledImgId(uint32_t id);

    /// @returns Index to resource in bindless array of Storage Images for access on GPU.
    GRACE_NODISCARD uint32_t GetStorageImgId() const;

    /// @returns Index to resource in bindless array of Sampled Images for access on GPU.
    GRACE_NODISCARD uint32_t GetSampledImgId() const;

    /// @returns `true` if associated `VkImage`, `VkImageView` or `VmaAllocation` are null.
    GRACE_NODISCARD bool IsNull() const;

    /// @returns `VkImage` of image which holds actual data.
    GRACE_NODISCARD const VkImage& GetImage() const;

    /// @returns `VkImageView` of image which tells you how the data is stored.
    GRACE_NODISCARD const ImageView& GetDefaultView() const;

    /// @returns Format per pixel of image.
    GRACE_NODISCARD const VkFormat& GetFormat() const;

    /// @returns VkExtent2D of image.
    GRACE_NODISCARD VkExtent2D GetExtent2D() const;

    /// @returns VkExtent3D of image.
    GRACE_NODISCARD const VkExtent3D& GetExtent3D() const;

    /// @returns Width of image.
    GRACE_NODISCARD uint32_t GetWidth() const;

    /// @returns Height of image.
    GRACE_NODISCARD uint32_t GetHeight() const;

    /// @returns Depth of image.
    GRACE_NODISCARD uint32_t GetDepth() const;

    GRACE_NODISCARD uint32_t GetMaxMipLevels() const;

    /// @returns Usage flags used to create image.
    GRACE_NODISCARD VkImageUsageFlags GetUsageFlags() const;

    /// @returns `VmaAllocation` which represents a single memory allocation.
    GRACE_NODISCARD const VmaAllocation& GetAllocation() const;

    /// @returns `VmaAllocationInfo` which stores metadata of the memory allocation e.g. allocation size.
    GRACE_NODISCARD VmaAllocationInfo2 GetAllocationInfo() const;

private:
    Device* m_Device = nullptr;
    ImageView m_DefaultView = {};
    VkImage m_Image = nullptr;
    VmaAllocation m_Allocation = nullptr;
    VkExtent3D m_Extent = {};
    VkFormat m_Format = {};
    VkImageUsageFlags m_UsageFlags = {};

    // For bindless
    uint32_t m_StorageImgId = 0;
    uint32_t m_SampledImgId = 0;

    bool m_IsSwapchainImage = false;
};

} // namespace Grace
