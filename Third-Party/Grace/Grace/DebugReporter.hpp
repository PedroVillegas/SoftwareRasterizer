#pragma once

#include <vulkan/vulkan.h>

namespace Grace
{

class DebugReporter
{
public:
    static void Check(VkResult result);
};

} // namespace Grace
