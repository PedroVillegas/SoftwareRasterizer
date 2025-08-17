#pragma once

#include <array>
#include <vector>

#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

class Image;
class Buffer;

/// Defines a bunch of potential resource usages
enum class AccessType : uint64_t
{
    None = 0, // No access. Useful primarily for initialization

    // Read access
    IndirectBuffer, // Read as an indirect buffer for drawing or dispatch
    IndexBuffer,    // Read as an index buffer for drawing
    VertexBuffer,   // Read as a vertex buffer for drawing

    VertexShaderUniformRead,              // Read as a uniform buffer in a vertex shader
    VertexShaderSampledRead,              // Read as a sampled image/uniform texel buffer in a vertex shader
    VertexShaderStorageRead,              // Read as any other resource in a vertex shader
    TessellationControlShaderUniformRead, // Read as a uniform buffer in a tessellation control shader
    TessellationControlShaderSampledRead, // Read as a sampled image/uniform texel buffer  in a tessellation control shader
    TessellationControlShaderStorageRead,    // Read as any other resource in a tessellation control shader
    TessellationEvaluationShaderUniformRead, // Read as a uniform buffer in a tessellation evaluation shader
    TessellationEvaluationShaderSampledRead, // Read as a sampled image/uniform texel buffer in a tessellation evaluation shader
    TessellationEvaluationShaderStorageRead,  // Read as any other resource in a tessellation evaluation shader
    GeometryShaderUniformRead,                // Read as a uniform buffer in a geometry shader
    GeometryShaderSampledRead,                // Read as a sampled image/uniform texel buffer  in a geometry shader
    GeometryShaderStorageRead,                // Read as any other resource in a geometry shader
    TaskShaderUniformRead,                    // Read as a uniform buffer in a task shader
    TaskShaderSampledRead,                    // Read as a sampled image/uniform texel buffer in a task shader
    TaskShaderStorageRead,                    // Read as any other resource in a task shader
    MeshShaderUniformRead,                    // Read as a uniform buffer in a mesh shader
    MeshShaderSampledRead,                    // Read as a sampled image/uniform texel buffer in a mesh shader
    MeshShaderStorageRead,                    // Read as any other resource in a mesh shader
    FragmentShaderUniformRead,                // Read as a uniform buffer in a fragment shader
    FragmentShaderSampledRead,                // Read as a sampled image/uniform texel buffer  in a fragment shader
    FragmentShaderColorAttachmentRead,        // Read as an input attachment in a fragment shader
    FragmentShaderDepthStencilAttachmentRead, // Read as an input attachment in a fragment shader
    FragmentShaderStorageRead,                // Read as any other resource in a fragment shader
    ColorAttachmentRead,                      // Read by standard blending/logic operations or subpass load operations
    DepthStencilAttachmentRead,               // Read by depth/stencil tests or subpass load operations
    ComputeShaderUniformRead,                 // Read as a uniform buffer in a compute shader
    ComputeShaderSampledRead,                 // Read as a sampled image/uniform texel buffer in a compute shader
    ComputeShaderStorageRead,                 // Read as any other resource in a compute shader
    AnyShaderUniformRead,
    AnyShaderSampledRead,
    AnyShaderStorageRead,

    CopyRead,    // Read as the source of a copy operation
    ResolveRead, // Read as the source of a resolve operation
    BlitRead,    // Read as the source of a blit operation
    ClearRead,   // Read as the source of a clear operation
    AnyTransferRead,
    HostRead, // Read on the host

    // Requires VK_KHR_swapchain to be enabled
    Present, // Read by the presentation engine (i.e. vkQueuePresentKHR)

    RayTracingShaderAccelerationStructureRead, // Read by a ray tracing shader as an acceleration structure
    AccelerationStructureBuildRead,            // Read as an acceleration structure during a build

    EndOfReadAccess,

    // Write access
    VertexShaderWrite,                 // Written as any resource in a vertex shader
    TessellationControlShaderWrite,    // Written as any resource in a tessellation control shader
    TessellationEvaluationShaderWrite, // Written as any resource in a tessellation evaluation shader
    GeometryShaderWrite,               // Written as any resource in a geometry shader

    TaskShaderWrite, // Written as any resource in a task shader
    MeshShaderWrite, // Written as any resource in a mesh shader

    FragmentShaderWrite,         // Written as any resource in a fragment shader
    ColorAttachmentWrite,        // Written as a color attachment during rendering, or via a subpass store op
    DepthStencilAttachmentWrite, // Written as a depth/stencil attachment during rendering, or via a subpass store op

    // Requires VK_KHR_maintenance2 to be enabled
    // DepthAttachmentWriteStencilReadOnly, // Written as a depth aspect of a depth/stencil attachment during rendering, whilst the stencil aspect is read-only
    // StencilAttachmentWriteDepthReadOnly, // Written as a stencil aspect of a depth/stencil attachment during rendering, whilst the depth aspect is read-only

    ComputeShaderWrite, // Written as any resource in a compute shader
    AnyShaderWrite,     // Written as any resource in any shader
    CopyWrite,          // Written as the destination of a copy operation
    ResolveWrite,       // Written as the destination of a resolve operation
    BlitWrite,          // Written as the destination of a blit operation
    ClearWrite,         // Written as the destination of a clear operation
    AnyTransferWrite,   // Written as the destination of any transfer operation

    HostPreinitialized, // Data pre-filled by host before device access starts
    HostWrite,          // Written on the host

    AccelerationStructureBuildWrite, // Written as an acceleration structure during a build

    ColorAttachmentReadWrite, // Read or written as a color attachment during rendering
    DepthStencilAttachmentReadWrite, // Read or written as a depth or stencil attachment during rendering

    // General access
    General, // Covers any access - useful for debug, generally avoid for performance reasons

    NumOfAccessTypes,
};

struct AccessInfo
{
    VkPipelineStageFlags2 stageMask;
    VkAccessFlags2 accessMask;
    VkImageLayout imageLayout;
};

static const std::array<AccessInfo, 68> AccessTypeMap = {
    { // AccessType::None
      { .stageMask = VK_PIPELINE_STAGE_2_NONE,
        .accessMask = VK_ACCESS_2_NONE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },

      // Read Access
      // AccessType::IndirectBuffer
      { .stageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        .accessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::IndexBuffer
      { .stageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
        .accessMask = VK_ACCESS_2_INDEX_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::VertexBuffer
      { .stageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
        .accessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },

      // AccessType::VertexShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::VertexShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::VertexShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::TessellationControlShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::TessellationControlShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::TessellationControlShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::TessellationEvaluationShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::TessellationEvaluationShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::TessellationEvaluationShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::GeometryShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::GeometryShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::GeometryShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::TaskShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::TaskShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::TaskShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::MeshShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::MeshShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::MeshShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::FragmentShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::FragmentShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::FragmentShaderColorAttachmentRead
      { .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::FragmentShaderDepthStencilAttachmentRead
      { .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL },
      // AccessType::FragmentShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::ColorAttachmentRead
      { .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
      // AccessType::DepthStencilAttachmentRead
      { .stageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL },

      // AccessType::ComputeShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::ComputeShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::ComputeShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::AnyShaderUniformRead
      { .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .accessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::AnyShaderSampledRead
      { .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL },
      // AccessType::AnyShaderStorageRead
      { .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::CopyRead
      { .stageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
      // AccessType::ResolveRead
      { .stageMask = VK_PIPELINE_STAGE_2_RESOLVE_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
      // AccessType::BlitRead
      { .stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
      // AccessType::ClearRead
      { .stageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },
      // AccessType::AnyTransferRead
      { .stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL },

      // AccessType::HostRead
      { .stageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .accessMask = VK_ACCESS_2_HOST_READ_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::Present
      { .stageMask = VK_PIPELINE_STAGE_2_NONE,
        .accessMask = VK_ACCESS_2_NONE,
        .imageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },

      // AccessType::RayTracingShaderAccelerationStructureRead
      { .stageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .accessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },
      // AccessType::AccelerationStructureBuildRead
      { .stageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .accessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },

      // AccessType::EndOfReadAccess
      { .stageMask = VK_PIPELINE_STAGE_2_NONE,
        .accessMask = VK_ACCESS_2_NONE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },

      // Write access
      // AccessType::VertexShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::TessellationControlShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::TessellationEvaluationShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::GeometryShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::TaskShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::MeshShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::FragmentShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::ColorAttachmentWrite
      { .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
      // AccessType::DepthStencilAttachmentWrite
      { .stageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
      // AccessType::ComputeShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },
      // AccessType::AnyShaderWrite
      { .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .accessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::CopyWrite
      { .stageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL },
      // AccessType::ResolveWrite
      { .stageMask = VK_PIPELINE_STAGE_2_RESOLVE_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL },
      // AccessType::BlitWrite
      { .stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL },
      // AccessType::ClearWrite
      { .stageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL },
      // AccessType::AnyTransferWrite
      { .stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        .accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL },

      // AccessType::HostPreinitialized
      { .stageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .accessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_PREINITIALIZED },
      // AccessType::HostWrite
      { .stageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
        .accessMask = VK_ACCESS_2_HOST_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::AccelerationStructureBuildWrite
      { .stageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .accessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED },

      // AccessType::ColorAttachmentReadWrite
      { .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL },

      // AccessType::DepthStencilAttachmentReadWrite
      { .stageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL },

      // AccessType::General
      { .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .accessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL },

      // AccessType::NumOfAccessTypes
      { .stageMask = VK_PIPELINE_STAGE_2_NONE,
        .accessMask = VK_ACCESS_2_NONE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED } }
};

enum class ImageLayout : uint8_t
{
    Optimal, // Choose the most optimal layout for each usage. Performs layout transitions as appropriate for the access.
    General, // Layout accessible by all Vulkan access types on a device - no layout transitions except for presentation

    // Requires VK_KHR_shared_presentable_image to be enabled. Can only be used for shared presentable images (i.e. single-buffered swap chains).
    GeneralAndPresentation // As GENERAL, but also allows presentation engines to access it - no layout transitions
};

/// Global barriers define a set of accesses on multiple resources at once.
/// If a buffer or image doesn't require a queue ownership transfer, or an image
/// doesn't require a layout transition (e.g. you're using one of the GENERAL
/// layouts) then a global barrier should be preferred.
/// Simply define the previous and next access types of resources affected.
struct MemoryBarrier
{
    std::vector<AccessType> accessesBefore;
    std::vector<AccessType> accessesAfter;
};

/// Buffer barriers should only be used for queue family ownership transfers
/// - otherwise, prefer global memory barriers
///
/// Access types are defined in the same way as for a global memory barrier, but
/// they only affect the buffer range identified by buffer, offset and size,
/// rather than all resources.
///
/// srcQueueFamilyIndex and dstQueueFamilyIndex will be passed unmodified into a
/// VkBufferMemoryBarrier.
///
/// A buffer barrier defining a queue ownership transfer needs to be executed
/// twice - once by a queue in the source queue family, and then once again by a
/// queue in the destination queue family, with a semaphore guaranteeing
/// execution order between them.
struct BufferBarrier
{
    VkBuffer buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
    std::vector<AccessType> accessesBefore;
    std::vector<AccessType> accessesAfter;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
};

/// Image barriers should only be used for queue family ownership transfers
/// or image layout transitions - otherwise, prefer global memory barriers
///
/// In general, it is better to use image barriers with THSVS_IMAGE_LAYOUT_OPTIMAL
/// than it is to use global barriers with images using either of the
/// THSVS_IMAGE_LAYOUT_GENERAL* layouts.
///
/// Access types are defined in the same way as for a global memory barrier, but
/// they only affect the image subresource range identified by image and
/// subresourceRange, rather than all resources.
///
/// srcQueueFamilyIndex, dstQueueFamilyIndex, image, and subresourceRange will
/// be passed unmodified into a VkImageMemoryBarrier.
///
/// An image barrier defining a queue ownership transfer needs to be executed
/// twice - once by a queue in the source queue family, and then once again by a
/// queue in the destination queue family, with a semaphore guaranteeing
/// execution order between them.
///
/// If discardContents is set to true, the contents of the image become
/// undefined after the barrier is executed, which can result in a performance
/// boost over attempting to preserve the contents.
/// This is particularly useful for transient images where the contents are
/// going to be immediately overwritten. A good example of when to use this is
/// when an application re-uses a presented image after vkAcquireNextImageKHR.
struct ImageBarrier
{
    VkImage image;
    VkImageSubresourceRange subresourceRange;
    std::vector<AccessType> accessesBefore;
    std::vector<AccessType> accessesAfter;
    ImageLayout prevLayout;
    ImageLayout nextLayout;
    VkBool32 discardContents;
    uint32_t srcQueueFamilyIndex;
    uint32_t dstQueueFamilyIndex;
};

class BarrierBuilder
{
public:
    BarrierBuilder& PipelineBarrier(VkCommandBuffer cmd);

    BarrierBuilder& AddMemoryBarrier(std::vector<AccessType>&& accessesBefore, std::vector<AccessType>&& accessesAfter);

    BarrierBuilder& AddImageBarrier(const Image& image,
                                    std::vector<AccessType>&& accessesBefore,
                                    std::vector<AccessType>&& accessesAfter);

    BarrierBuilder& AddBufferBarrier(const Buffer& buffer,
                                     std::vector<AccessType>&& accessesBefore,
                                     std::vector<AccessType>&& accessesAfter);

private:
    /// Translates a Grace::MemoryBarrier into a VkMemoryBarrier2
    static void GetVulkanMemoryBarrier(const MemoryBarrier& barrier, VkMemoryBarrier2& vkBarrierOut) ;

    /// Translates a Grace::BufferBarrier into a VkBufferMemoryBarrier2
    static void GetVulkanBufferMemoryBarrier(const BufferBarrier& barrier, VkBufferMemoryBarrier2& vkBarrierOut) ;

    /// Translates a Grace::ImageBarrier into a VkImageMemoryBarrier2
    static void GetVulkanImageMemoryBarrier(const ImageBarrier& barrier, VkImageMemoryBarrier2& vkBarrierOut) ;

private:
    MemoryBarrier m_MemoryBarrier = {};
    std::vector<ImageBarrier> m_ImageBarriers = {};
    std::vector<BufferBarrier> m_BufferBarriers = {};
};

} // namespace Grace
