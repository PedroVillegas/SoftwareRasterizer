#pragma once

#define GRACE_NODISCARD [[nodiscard]]
#define GRACE_FALLTHROUGH [[fallthrough]]

#define GRACE_LOAD_INSTANCE_PFN(instance, fn) reinterpret_cast<PFN_##fn>(vkGetInstanceProcAddr(instance, #fn))
#define GRACE_LOAD_DEVICE_PFN(device, fn) reinterpret_cast<PFN_##fn>(vkGetDeviceProcAddr(device, #fn))

#ifdef _DEBUG
#define GRACE_SET_VK_DEBUG_NAME vkSetDebugUtilsObjectNameEXT_Meta
#else
#define GRACE_SET_VK_DEBUG_NAME(...) ((void) 0)
#endif

#define GRACE_DEFINE_RESOURCE_REGISTRY(ResourceType, RegistryName) \
    Registry<ResourceType> RegistryName;                           \
    template <>                                                    \
    auto& ResourceRegistry<ResourceType>()                         \
    {                                                              \
        return RegistryName;                                       \
    };

#define GRACE_DEFINE_RESOURCE_HANDLE(ResourceType) \
    class ResourceType;                            \
    using ResourceType##Handle = Handle<ResourceType>;

#define GRACE_DEFINE_TEMPLATED_RESOURCE_HANDLE(ResourceType, TemplateType, Alias) \
    template <typename T>                                                         \
    class ResourceType;                                                           \
    using Alias##Handle = Handle<ResourceType<TemplateType>>;
