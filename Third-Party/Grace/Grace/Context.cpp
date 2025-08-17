#include "Context.hpp"

#include <Grace/DebugReporter.hpp>
#include <Grace/HelperFunctions.hpp>

#include <cstring>

#ifdef GRACE_USE_GLFW
#include <GLFW/glfw3.h>
#endif

namespace Grace
{

VkInstance& Context::GetInstance()
{
    return m_Instance;
}

Context::~Context()
{
    m_Device->WaitIdle();
    m_Device.reset();
    vkDestroyInstance(m_Instance, nullptr);
}

Context::Context(const ContextDesc& desc)
{
    vkSetDebugUtilsObjectNameEXT_Meta = nullptr;
    vkCmdBeginDebugUtilsLabelEXT_Meta = nullptr;
    vkCmdEndDebugUtilsLabelEXT_Meta = nullptr;
    vkCmdInsertDebugUtilsLabelEXT_Meta = nullptr;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    uint32_t apiVersion = VK_API_VERSION_1_3;

#if (GRACE_TARGET_VULKAN_API_VERSION == 13)
    apiVersion = VK_API_VERSION_1_3;
#elif (GRACE_TARGET_VULKAN_API_VERSION == 14)
    apiVersion = VK_API_VERSION_1_4
#else
    vkEnumerateInstanceVersion(&apiVersion);
    assert(apiVersion >= VK_API_VERSION_1_3);
#endif

    appInfo.apiVersion = apiVersion;

    VkInstanceCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pNext = nullptr;
    ici.flags = 0;
    ici.pApplicationInfo = &appInfo;
    ici.enabledLayerCount = 0;

    std::vector<const char*> extensions = GetRequiredExtensions();
    extensions.insert(extensions.end(), desc.extensions.begin(), desc.extensions.end());
    ici.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();

    DebugReporter::Check(vkCreateInstance(&ici, nullptr, &m_Instance));

    vkSetDebugUtilsObjectNameEXT_Meta = GRACE_LOAD_INSTANCE_PFN(m_Instance, vkSetDebugUtilsObjectNameEXT);
    vkCmdBeginDebugUtilsLabelEXT_Meta = GRACE_LOAD_INSTANCE_PFN(m_Instance, vkCmdBeginDebugUtilsLabelEXT);
    vkCmdEndDebugUtilsLabelEXT_Meta = GRACE_LOAD_INSTANCE_PFN(m_Instance, vkCmdEndDebugUtilsLabelEXT);
    vkCmdInsertDebugUtilsLabelEXT_Meta = GRACE_LOAD_INSTANCE_PFN(m_Instance, vkCmdInsertDebugUtilsLabelEXT);

    m_Device = std::make_unique<Device>(m_Instance, desc.deviceConfig);

    AssignDebugName<VkInstance>(m_Device->GetVkHandle(), m_Instance, "Grace::Instance");
    AssignDebugName<VkDevice>(m_Device->GetVkHandle(), m_Device->GetVkHandle(), "Grace::Device");
}

Device* Context::GetDevicePtr()
{
    return m_Device.get();
}

std::vector<const char*> Context::GetRequiredExtensions() const
{
#ifdef GRACE_USE_GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
#endif

    // Checking for supported extensions
    uint32_t extensionsCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);

    std::vector<VkExtensionProperties> availableInstanceExtensions(extensionsCount);

    vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, availableInstanceExtensions.data());

    std::vector<const char*> extensions
#ifdef GRACE_USE_GLFW
        (glfwExtensions, glfwExtensions + glfwExtensionCount)
#endif
            ;

    for (auto& availableExt : availableInstanceExtensions)
    {
        if (strcmp(availableExt.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

} // namespace Grace
