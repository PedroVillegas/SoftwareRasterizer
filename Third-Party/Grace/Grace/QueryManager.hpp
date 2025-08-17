#pragma once

#include <array>
#include <cassert>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>

#include <vulkan/vulkan.h>
#include <Grace/GraceExport.h>
#include <Grace/Macros.hpp>

namespace Grace
{

class Device;

enum class QueryWriteFlags : uint32_t
{
    None = 0,
    WriteIfPreviousResultIsAvailable,
};

struct GRACE_EXPORT QueryGroupDesc
{
    uint32_t timestampQueriesCount = 256U;
    uint32_t occlusionQueriesCount = 256U;
    uint32_t pipelineStatisticsCount = 32U;
    VkQueryPipelineStatisticFlags pipelineStatisticsFlags;
};

namespace TimestampUnits
{

template <double Val>
struct GRACE_EXPORT DurationUnits
{
    static constexpr double value = Val;
};

using Nanoseconds = DurationUnits<1.0>;
using Microseconds = DurationUnits<1E-03>;
using Milliseconds = DurationUnits<1E-06>;
using Seconds = DurationUnits<1E-09>;

} // namespace TimestampUnits

namespace QueryType
{

struct QueryTypeBase
{
};

struct GRACE_EXPORT Timestamp : QueryTypeBase
{
};

struct GRACE_EXPORT Occlusion : QueryTypeBase
{
};

struct GRACE_EXPORT PipelineStatistics : QueryTypeBase
{
};

} // namespace QueryType

template <typename QueryTy>
class GRACE_EXPORT QueryGroup
{
    static_assert(std::derived_from<QueryTy, QueryType::QueryTypeBase>,
                  "QueryGroup type must be derived from QueryGroupType::QueryTypeBase!");

public:
    ~QueryGroup() = default;
    QueryGroup() = default;

    QueryGroup(const QueryGroup&) = delete;
    QueryGroup& operator=(const QueryGroup&) = delete;

    QueryGroup(QueryGroup&& other) noexcept = delete;
    QueryGroup& operator=(QueryGroup&& other) noexcept = delete;

    GRACE_NODISCARD VkQueryPool GetVkQueryPool() const
    {
        return m_QueryPool;
    }

    GRACE_NODISCARD std::vector<uint64_t>& GetQueries()
    {
        return m_Queries;
    }

    GRACE_NODISCARD uint64_t GetQuery(const char* name, uint32_t relativeBit = 0U, uint32_t frameIndex = 0U) const
    {
        const uint32_t offset = GetQueryOffset(name);
        return m_Queries[(m_Range * frameIndex) + offset + relativeBit];
    }

    void
    GetQueryIfAvailable(uint64_t& inout, const char* name, uint32_t relativeBit = 0U, uint32_t frameIndex = 0U) const
    {
        const uint32_t offset = GetQueryOffset(name);
        const uint32_t queryOffsetMultiFrame = ((m_Range - 1) * frameIndex) + (offset * (m_ValuesPerQuery + 1));
        // The query's availability bit immediately proceeds the n values of the query
        if (m_Queries[queryOffsetMultiFrame + 1] != 0)
        {
            inout = m_Queries[queryOffsetMultiFrame + relativeBit];
        }
    }

    GRACE_NODISCARD uint32_t GetQueryOffset(const char* name) const
    {
        assert(name != nullptr);
        assert(m_NamedQueryMap.contains(name));
        return m_NamedQueryMap.at(name);
    }

    GRACE_NODISCARD uint32_t GetRange() const
    {
        return m_Range;
    }

    GRACE_NODISCARD uint32_t GetQueryCount() const
    {
        return m_QueriesWrittenSinceLastReset;
    }

    GRACE_NODISCARD uint32_t GetValuesPerQuery() const
    {
        return m_ValuesPerQuery;
    }

    template <typename UnitsType, typename U = QueryTy>
    GRACE_NODISCARD std::enable_if_t<std::is_same_v<U, QueryType::Timestamp>, double>
    Duration(const char* start, const char* end, uint32_t frameIndex = 0U) const
    {
        static_assert(std::is_same_v<UnitsType, TimestampUnits::Nanoseconds>
                      || std::is_same_v<UnitsType, TimestampUnits::Microseconds>
                      || std::is_same_v<UnitsType, TimestampUnits::Milliseconds>
                      || std::is_same_v<UnitsType, TimestampUnits::Seconds>);

        double duration = static_cast<double>(GetQuery(end, 0, frameIndex) - GetQuery(start, 0, frameIndex))
                        * static_cast<double>(m_TimestampPeriod) * UnitsType::value;

        return duration;
    }

    template <typename UnitsType, typename U = QueryTy>
    GRACE_NODISCARD std::enable_if_t<std::is_same_v<U, QueryType::Timestamp>, void>
    DurationIfAvailable(double& inout, const char* start, const char* end, uint32_t frameIndex = 0U) const
    {
        static_assert(std::is_same_v<UnitsType, TimestampUnits::Nanoseconds>
                      || std::is_same_v<UnitsType, TimestampUnits::Microseconds>
                      || std::is_same_v<UnitsType, TimestampUnits::Milliseconds>
                      || std::is_same_v<UnitsType, TimestampUnits::Seconds>);

        uint64_t startTicks = 0;
        uint64_t endTicks = 0;

        GetQueryIfAvailable(startTicks, start, 0, frameIndex);
        GetQueryIfAvailable(endTicks, end, 0, frameIndex);

        if (startTicks > 0 && endTicks > 0)
        {
            inout =
                static_cast<double>(endTicks - startTicks) * static_cast<double>(m_TimestampPeriod) * UnitsType::value;
        }
    }

private:
    GRACE_NODISCARD uint32_t AddQuery(const char* name)
    {
        const uint32_t offset = m_QueriesWrittenSinceLastReset++;
        m_NamedQueryMap[name] = offset;
        return offset;
    }

private:
    VkQueryPool m_QueryPool = nullptr;
    std::vector<uint64_t> m_Queries = {};
    // Only need one named query map per query group since query indices are constant
    std::unordered_map<std::string, uint32_t> m_NamedQueryMap = {};
    uint32_t m_QueriesWrittenSinceLastReset = 0U;
    uint32_t m_ValuesPerQuery = 1U;
    uint32_t m_Range = 0U;
    float m_TimestampPeriod = 0.0F;

    friend class QueryManager;
};

using TimestampQueryGroup = QueryGroup<QueryType::Timestamp>;
using OcclusionQueryGroup = QueryGroup<QueryType::Occlusion>;
using PipelineStatsQueryGroup = QueryGroup<QueryType::PipelineStatistics>;

class QueryManager
{
public:
    ~QueryManager();
    QueryManager() = default;
    QueryManager(Device* pDevice, uint32_t framesInFlight, const QueryGroupDesc& qgDesc);

    QueryManager(const QueryManager&) = delete;
    QueryManager& operator=(const QueryManager&) = delete;

    QueryManager(QueryManager&& other) noexcept = delete;
    QueryManager& operator=(QueryManager&& other) noexcept = delete;

    template <typename T>
    GRACE_NODISCARD QueryGroup<T>& GetQueryGroup()
    {
        static_assert(std::derived_from<T, QueryType::QueryTypeBase>,
                      "QueryGroup type must be derived from QueryGroupType::QueryTypeBase!");

        if constexpr (std::is_same_v<T, QueryType::Timestamp>)
        {
            return m_TimestampQueryGroup;
        }
        else if constexpr (std::is_same_v<T, QueryType::Occlusion>)
        {
            return m_OcclusionQueryGroup;
        }
        else if constexpr (std::is_same_v<T, QueryType::PipelineStatistics>)
        {
            return m_PipelineStatsQueryGroup;
        }
    }

    template <typename T>
    GRACE_NODISCARD uint32_t AddQuery(const char* name)
    {
        QueryGroup<T>& qg = GetQueryGroup<T>();
        return qg.AddQuery(name);
    }

    template <typename T>
    void ResetQueryGroup()
    {
        QueryGroup<T>& qg = GetQueryGroup<T>();
        qg.m_QueriesWrittenSinceLastReset = 0;
    }

private:
    Device* m_pDevice = nullptr;
    TimestampQueryGroup m_TimestampQueryGroup = {};
    OcclusionQueryGroup m_OcclusionQueryGroup = {};
    PipelineStatsQueryGroup m_PipelineStatsQueryGroup = {};
};

} // namespace Grace