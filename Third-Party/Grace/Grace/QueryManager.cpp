#include "QueryManager.hpp"

#include <Grace/Device.hpp>
#include <Grace/DebugReporter.hpp>
#include <Grace/HelperFunctions.hpp>

namespace Grace
{

QueryManager::~QueryManager()
{
    vkDestroyQueryPool(m_pDevice->GetVkHandle(), m_TimestampQueryGroup.m_QueryPool, nullptr);
    vkDestroyQueryPool(m_pDevice->GetVkHandle(), m_OcclusionQueryGroup.m_QueryPool, nullptr);
    vkDestroyQueryPool(m_pDevice->GetVkHandle(), m_PipelineStatsQueryGroup.m_QueryPool, nullptr);
}

QueryManager::QueryManager(Device* pDevice, uint32_t framesInFlight, const QueryGroupDesc& qgDesc) : m_pDevice(pDevice)
{
    assert(m_pDevice != nullptr);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_pDevice->GetPhysicalDevice(), &props);
    const float timestampPeriod = props.limits.timestampPeriod;

    // Initialise Timestamp Query Group
    m_TimestampQueryGroup.m_Queries.resize(qgDesc.timestampQueriesCount * framesInFlight);
    m_TimestampQueryGroup.m_TimestampPeriod = timestampPeriod;
    m_TimestampQueryGroup.m_Range = qgDesc.timestampQueriesCount;

    VkQueryPoolCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    pci.pNext = nullptr;
    pci.queryType = VK_QUERY_TYPE_TIMESTAMP;
    pci.queryCount = m_TimestampQueryGroup.m_ValuesPerQuery * m_TimestampQueryGroup.m_Queries.size();
    pci.flags = 0;
    pci.pipelineStatistics = 0;
    DebugReporter::Check(vkCreateQueryPool(m_pDevice->GetVkHandle(), &pci, nullptr, &m_TimestampQueryGroup.m_QueryPool));

    AssignDebugName<VkQueryPool>(pDevice->GetVkHandle(), m_TimestampQueryGroup.m_QueryPool, "Grace::QueryPool::Timestamp");

    // Initialise Occlusion Query Group
    m_OcclusionQueryGroup.m_Queries.resize(qgDesc.occlusionQueriesCount * framesInFlight);
    m_OcclusionQueryGroup.m_TimestampPeriod = timestampPeriod;
    m_OcclusionQueryGroup.m_Range = qgDesc.occlusionQueriesCount;

    pci.queryType = VK_QUERY_TYPE_OCCLUSION;
    pci.queryCount = m_OcclusionQueryGroup.m_ValuesPerQuery * m_OcclusionQueryGroup.m_Queries.size();
    DebugReporter::Check(vkCreateQueryPool(m_pDevice->GetVkHandle(), &pci, nullptr, &m_OcclusionQueryGroup.m_QueryPool));

    AssignDebugName<VkQueryPool>(pDevice->GetVkHandle(), m_OcclusionQueryGroup.m_QueryPool, "Grace::QueryPool::Occlusion");

    // Need to find the number of pipelineStatistics bits that have been set
    uint32_t bitsSet = 0;
    uint32_t numOfPossibleBitsSet = 14U; // As found in VkQueryPipelineStatisticFlagBits enum as of v1.4.313.0
    for (uint32_t i = 0; i < numOfPossibleBitsSet; ++i)
    {
        uint32_t possibleBitsSetMask = 1U << i;
        if (qgDesc.pipelineStatisticsFlags & possibleBitsSetMask)
        {
            bitsSet++;
        }
    }

    // Initialise Pipeline Stats Query Group
    m_PipelineStatsQueryGroup.m_Queries.resize(bitsSet * qgDesc.pipelineStatisticsCount * framesInFlight);
    m_PipelineStatsQueryGroup.m_ValuesPerQuery = bitsSet;
    m_PipelineStatsQueryGroup.m_TimestampPeriod = timestampPeriod;
    m_PipelineStatsQueryGroup.m_Range = qgDesc.pipelineStatisticsCount;

    pci.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
    pci.queryCount = m_PipelineStatsQueryGroup.m_ValuesPerQuery * m_PipelineStatsQueryGroup.m_Queries.size();
    pci.pipelineStatistics = qgDesc.pipelineStatisticsFlags;
    DebugReporter::Check(vkCreateQueryPool(m_pDevice->GetVkHandle(), &pci, nullptr, &m_PipelineStatsQueryGroup.m_QueryPool));

    AssignDebugName<VkQueryPool>(
        pDevice->GetVkHandle(), m_PipelineStatsQueryGroup.m_QueryPool, "Grace::QueryPool::PipelineStatistics");
}

} // namespace Grace