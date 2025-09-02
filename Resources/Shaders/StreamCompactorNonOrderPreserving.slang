// Divides the input elements into many 1024-element sized groups.
// Each group is further divided into 16 subgroups and each subgroup has 64 elements.
// This allows each group to be processed by a single 64-thread wave.
// A single workgroup invocation can therefore process upto 1024x1024 = 1,048,576 elements.

#include "Shared.slangi"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

static const uint waveSize = WaveGetLaneCount();
groupshared uint smemInterWaveOffset;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    uint N = PushConst.verticesCount;
    if (id.x >= (max(N, 1024) >> 10) << 6) return;

    // if (id.x == 0) printf("# Active Threads = %u", (max(N, 1024) >> 10) << 6);

    if (id.x == 0) smemInterWaveOffset = 0;
    GroupMemoryBarrierWithGroupSync();

    uint warpId = id.x >> 6;
    uint laneId = id.x % waveSize;
    uint blockOffset = warpId << 10;

    uint2 predmask = uint2(0);
    uint2 counts = uint2(0);
    uint count = 0;
    // Stage 1 - count and predicateMask for each block
    for (uint i = 0; i < 16; i++)
    {
        float el = float(CULLED_VERTEX);

        // Fetch 64 elements, 1 per lane
        uint index = blockOffset + (i << 6) + laneId;
        if (index < N)
        {
            el = PushConst.intrinsicDataBuffer[index].position.x;
        }

        // Predicate mask
        bool p = el != float(CULLED_VERTEX);
        uint2 mask = WaveActiveBallot(p).xy;

        // Lane 0 stores the count and predicate mask of elements 0...63,
        // Lane 1 stores the count and predicate mask of elements 64...127 etc.
        if (laneId == i)
        {
            predmask = mask;
            counts = countbits(predmask);
            count = counts.x + counts.y;
        }
    }

    // if (laneId < 16 && warpId == 0)
    //         printf("thread: %u, count: %u\n", id.x, count);

    // Stage 2 - Prefix sum on count
    for (uint i = 1; i < 16; i++)
    {
        int n = WaveReadLaneAt(count, i - 1);
        if (laneId == i) count += n;
    }

    // if (laneId == 15)
    //     printf("thread: %u, count: %u\n", id.x, count);

    // Stage 3 - compute interWaveOffset
    uint orig;
    if (laneId == 15)
    {
        // printf("thread: %u, count: %u\n", id.x, count);
        InterlockedAdd(smemInterWaveOffset, count, orig);
    }

    uint interWaveOffset = WaveReadLaneAt(orig, 15);

    // if (laneId == 0 && warpId == 0)
    //     printf("thread: %u, count: %u\n", id.x, interWaveOffset);
    // if (laneId == 0 && warpId == 1)
    //     printf("thread: %u, count: %u\n", id.x, interWaveOffset);

    for (uint i = 0; i < 16; i++)
    {
        uint intraBlockOffset = 0;
        uint2 mask = WaveReadLaneAt(predmask, i);
        uint2 counts64 = WaveReadLaneAt(counts, i);
        uint index = blockOffset + (i << 6) + laneId;
        uint interBlockOffset = 0;
        if (i > 0) interBlockOffset = WaveReadLaneAt(count, i - 1);

        if (laneId < 32)
        {
            uint predtest = 1 << laneId;
            if ((mask.x & predtest) > 0)
            {
                intraBlockOffset = countbits(mask.x & (predtest - 1));
                PushConst.compactedIntrinsicDataBuffer[interWaveOffset + interBlockOffset + intraBlockOffset] = PushConst.intrinsicDataBuffer[index];
            }
        }
        else
        {
            uint predtest = 1 << (laneId - 32);
            if ((mask.y & predtest) > 0)
            {
                intraBlockOffset = counts64.x + countbits(mask.y & (predtest - 1));
                PushConst.compactedIntrinsicDataBuffer[interWaveOffset + interBlockOffset + intraBlockOffset] = PushConst.intrinsicDataBuffer[index];
            }
        }
    }

    if (laneId == 15) InterlockedAdd(PushConst.metaDataBuffer[0], interWaveOffset + count);
}
