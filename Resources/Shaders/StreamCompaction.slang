#pragma DO_NOT_COMPILE

#ifndef _STREAM_COMPACTION_H_
#define _STREAM_COMPACTION_H_

// Divides the input elements into many 1024-element sized groups.
// Each group is further divided into 16 subgroups and each subgroup has 64 elements.
// This allows each group to be processed by a single 64-thread wave.
// A single workgroup invocation can therefore process upto 1024x1024 = 1,048,576 elements.

groupshared uint smemInterWaveOffset;

interface StreamCompactionPredicateEvaluationFn
{
    associatedtype PRED_TYPE : IArithmetic;
    associatedtype BUFFER_ELEM_TYPE;

    bool EvaluatePredicate(const PRED_TYPE item);

    PRED_TYPE GetPredicateElement(BUFFER_ELEM_TYPE* src, const uint index);
};

__generic<ELEM_TYPE, PRED_EVAL_FN>
void StreamCompactionNonOrderPreserving(const uint globalThreadId, ELEM_TYPE* src, ELEM_TYPE* dst,
                                        const uint srcOffset, const uint srcSize,
                                        __ref uint globalMemDstSize, PRED_EVAL_FN predEvalFn)
    where PRED_EVAL_FN : StreamCompactionPredicateEvaluationFn
    where ELEM_TYPE == PRED_EVAL_FN.BUFFER_ELEM_TYPE
{
    if (globalThreadId >= (max(srcSize, 1024) >> 10) << 7) return;

    if (globalThreadId == 0) smemInterWaveOffset = 0;
    GroupMemoryBarrierWithGroupSync();

    uint waveSize = 64;
    uint warpId = globalThreadId >> 6; // 2^6 = 64 for 64-wide wave
    uint laneId = globalThreadId % waveSize;
    uint blockOffset = warpId << 10;

    uint2 predmask = uint2(0);
    uint2 counts = uint2(0);
    uint count = 0;

    // Stage 1 - count and predicateMask for each block
    for (uint i = 0; i < 16; i++)
    {
        bool predicate = false;

        // Fetch 64 elements, 1 per lane
        uint index = blockOffset + (i << 6) + laneId;
        if (index < srcSize)
        {
            PRED_EVAL_FN.PRED_TYPE item = predEvalFn.GetPredicateElement(Ptr<PRED_EVAL_FN.BUFFER_ELEM_TYPE>(src), srcOffset + index);
            predicate = predEvalFn.EvaluatePredicate(item);
        }

        // Predicate mask
        uint2 mask = WaveActiveBallot(predicate).xy;

        // Lane 0 stores the count and predicate mask of elements 0...63,
        // Lane 1 stores the count and predicate mask of elements 64...127 etc.
        if (laneId == i)
        {
            predmask = mask;
            counts = countbits(predmask);
            count = counts.x + counts.y;
        }
    }

    // Stage 2 - Prefix sum on count
    for (uint i = 1; i < 16; i++)
    {
        int n = WaveReadLaneAt(count, i - 1);
        if (laneId == i) count += n;
    }

    // Stage 3 - compute interWaveOffset
    uint orig;
    if (laneId == 15)
    {
        InterlockedAdd(smemInterWaveOffset, count, orig);
    }

    uint interWaveOffset = WaveReadLaneAt(orig, 15);

    uint dstBufferOffset = 0;
    if (laneId == 15)
    {
        InterlockedAdd(globalMemDstSize, count, dstBufferOffset);
        // printf("dstBufferOffset: %u, count: %u\n", dstBufferOffset, count);
    }
    dstBufferOffset = 0; //WaveReadLaneAt(dstBufferOffset, 15);

    if (warpId == 0 && laneId == 15)
    {
        printf("dstBufferOffset: %u, interWaveOffset: %u, count: %u\n", dstBufferOffset, interWaveOffset, count);
    }

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
                dst[dstBufferOffset + interWaveOffset + interBlockOffset + intraBlockOffset] = src[srcOffset + index];
                // printf("dst index: %u \n", dstBufferOffset + interWaveOffset + interBlockOffset + intraBlockOffset);
            }
        }
        else
        {
            uint predtest = 1 << (laneId - 32);
            if ((mask.y & predtest) > 0)
            {
                intraBlockOffset = counts64.x + countbits(mask.y & (predtest - 1));
                dst[dstBufferOffset + interWaveOffset + interBlockOffset + intraBlockOffset] = src[srcOffset + index];
            }
        }
    }
}

#endif // _STREAM_COMPACTION_H_
