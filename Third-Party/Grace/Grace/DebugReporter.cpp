#include "DebugReporter.hpp"

#include <string>
#include <iostream>

#include <vulkan/vulkan.h>

namespace Grace
{

void DebugReporter::Check(VkResult result)
{
    std::string errorMessage = "";

    // VkResult messages taken from https://registry.khronos.org/vulkan/specs/latest/man/html/VkResult.html
    // Success codes have been omitted

    // clang-format off
    switch (result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        errorMessage = "VK_ERROR_OUT_OF_HOST_MEMORY: A host memory allocation has failed";
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        errorMessage = "VK_ERROR_OUT_OF_DEVICE_MEMORY: A device memory allocation has failed";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        errorMessage = "VK_ERROR_INITIALIZATION_FAILED: Initialization of an object could not be completed";
        break;
    case VK_ERROR_DEVICE_LOST:
        errorMessage = "VK_ERROR_DEVICE_LOST: The logical or physical device has been lost";
        break;
    case VK_ERROR_MEMORY_MAP_FAILED:
        errorMessage = "VK_ERROR_MEMORY_MAP_FAILED: Mapping of a memory object has failed";
        break;
    case VK_ERROR_LAYER_NOT_PRESENT:
        errorMessage = "VK_ERROR_LAYER_NOT_PRESENT: A requested layer is not present or could not be loaded";
        break;
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        errorMessage = "VK_ERROR_EXTENSION_NOT_PRESENT: A requested extension is not supported";
        break;
    case VK_ERROR_FEATURE_NOT_PRESENT:
        errorMessage = "VK_ERROR_FEATURE_NOT_PRESENT: A requested feature is not supported";
        break;
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        errorMessage = "VK_ERROR_INCOMPATIBLE_DRIVER: The requested version of Vulkan is not supported by the driver or is otherwise incompatible for implementation-specific reasons";
        break;
    case VK_ERROR_TOO_MANY_OBJECTS:
        errorMessage = "VK_ERROR_TOO_MANY_OBJECTS: Too many objects of a type have already been created";
        break;
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        errorMessage = "VK_ERROR_FORMAT_NOT_SUPPORTED: A requested format is not supported on this device";
        break;
    case VK_ERROR_FRAGMENTED_POOL:
        errorMessage = "VK_ERROR_FRAGMENTED_POOL: A pool allocation has failed due to fragmentation of the pool’s memory. This must only be returned if no attempt to allocate host or device memory was made to accommodate the new allocation. This should be returned in preference to VK_ERROR_OUT_OF_POOL_MEMORY, but only if the implementation is certain that the pool allocation failure was due to fragmentation";
        break;
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        errorMessage = "VK_ERROR_OUT_OF_POOL_MEMORY: A pool memory allocation has failed. This must only be returned if no attempt to allocate host or device memory was made to accommodate the new allocation. If the failure was definitely due to fragmentation of the pool, VK_ERROR_FRAGMENTED_POOL should be returned instead";
        break;
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        errorMessage = "VK_ERROR_INVALID_EXTERNAL_HANDLE: An external handle is not a valid handle of the specified type";
        break;
    case VK_ERROR_FRAGMENTATION:
        errorMessage = "VK_ERROR_FRAGMENTATION: A descriptor pool creation has failed due to fragmentation";
        break;
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        errorMessage = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: A buffer creation or memory allocation failed because the requested address is not available. A shader group handle assignment failed because the requested shader group handle information is no longer valid";
        break;
#if (GRACE_TARGET_VULKAN_API_VERSION >= 14)
    case VK_ERROR_NOT_PERMITTED:
        errorMessage = "VK_ERROR_NOT_PERMITTED: The driver implementation has denied a request to acquire a priority above the default priority (VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT) because the application does not have sufficient privileges";
        break;
#endif
    case VK_ERROR_SURFACE_LOST_KHR:
        errorMessage = "VK_ERROR_SURFACE_LOST_KHR: A surface is no longer available";
        break;
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        errorMessage = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: The requested window is already connected to a VkSurfaceKHR, or to some other non-Vulkan API";
        break;
    case VK_ERROR_OUT_OF_DATE_KHR:
        errorMessage = "VK_ERROR_OUT_OF_DATE_KHR: A surface has changed in such a way that it is no longer compatible with the swapchain, and further presentation requests using the swapchain will fail";
        break;
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        errorMessage = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: The display used by a swapchain does not use the same presentable image layout, or is incompatible in a way that prevents sharing an image";
        break;
    case VK_ERROR_VALIDATION_FAILED_EXT:
        errorMessage = "VK_ERROR_VALIDATION_FAILED_EXT: A command failed because invalid usage was detected by the implementation or a validation-layer";
        break;
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
        errorMessage = "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: The requested VkImageUsageFlags are not supported";
        break;
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
        errorMessage = "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: The requested video picture layout is not supported";
        break;
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
        errorMessage = "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: A video profile operation specified via VkVideoProfileInfoKHR::videoCodecOperation is not supported";
        break;
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
        errorMessage = "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: Format parameters in a requested VkVideoProfileInfoKHR chain are not supported";
        break;
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
        errorMessage = "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: Codec-specific parameters in a requested VkVideoProfileInfoKHR chain are not supported";
        break;
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
        errorMessage = "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: The specified video Std header version is not supported";
        break;
    case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
        errorMessage = "VK_ERROR_NOT_ENOUGH_SPACE_KHR: The application did not provide enough space to return all the required data";
        break;
    case VK_ERROR_UNKNOWN:
        errorMessage = "VK_ERROR_UNKNOWN: An unknown error has occurred; either the application has provided invalid input, or an implementation failure has occurred";
        break;

    default:
        break;
    }
    // clang-format on

    if (result < VK_SUCCESS)
    {
        std::cout << "\033[1;31m[ERROR]\033[0m " + errorMessage << std::endl;
        //abort();
    }
}

} // namespace Grace
