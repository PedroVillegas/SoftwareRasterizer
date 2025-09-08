// The Ultra-Coarse Rasterizer takes a buffer of processed, visible triangles
// and rasterizes them into bins, collections of tiles which cover the
// viewport.
//
// Each workgroup dispatched is responsible for a single bin and within
// the workgroup each thread will consume and process a triangle until
// all triangles have been processed.

#include "Shared.slang"
#include "RasterizerUtility.slang"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

groupshared uint smemPerBinPrimitiveCounter;
groupshared uint smemTrianglesProcessed;
groupshared uint[MAX_PRIMITIVE_BITMAP_BLOCK_COUNT] smemPrimitiveBitmapBlocks;

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID,
          uint3 workgroupId : SV_GroupID,
          uint3 localThreadId : SV_GroupThreadID)
{
    const uint intrinsicDataBufferSize = PushConst.metaDataBuffer[0].compactedIntrinsicDataBufferSize;
    uint triangleCount = intrinsicDataBufferSize / 3;
    smemPerBinPrimitiveCounter = 0;
    smemTrianglesProcessed = 0;

    const uint linearLocalThreadId = (localThreadId.y * WorkgroupSize().x) + localThreadId.x;
    smemPrimitiveBitmapBlocks[linearLocalThreadId] = 0;

    GroupMemoryBarrierWithGroupSync();

    // Bounds of bin in raster space
    int2 halfFramebuffer = int2(uLongLifeGlobals.framebufferSize * float2(0.5));
    int2 mi = max(int2(workgroupId.xy * int2(uLongLifeGlobals.ultraCoarseRasterTileSize)) - halfFramebuffer, -halfFramebuffer);
    int2 ma = min(mi + int2(uLongLifeGlobals.ultraCoarseRasterTileSize), halfFramebuffer);
    Region2D region = Region2D(mi, ma);

    while (smemTrianglesProcessed < triangleCount)
    {
        // TODO: Would this work?? Probably faster because triangles will be processed at the same time since no atomics

        // uint triangleBeginOffset = (smemTrianglesProcessed + linearLocalThreadId) * 3;
        // do stuff...
        // smemTrianglesProcessed++;
        // GroupMemoryBarrierWithGroupSync();

        uint orig;
        InterlockedAdd(smemTrianglesProcessed, 1, orig);
        uint triangleBeginOffset = orig * 3;

        if (triangleBeginOffset + 2 >= intrinsicDataBufferSize) break;

        // Fetch triangle
        int2 vtxPosRasterSpace_0 = PushConst.compactedIntrinsicDataBuffer[triangleBeginOffset].position.xy;
        int2 vtxPosRasterSpace_1 = PushConst.compactedIntrinsicDataBuffer[triangleBeginOffset + 1].position.xy;
        int2 vtxPosRasterSpace_2 = PushConst.compactedIntrinsicDataBuffer[triangleBeginOffset + 2].position.xy;

        Triangle triangle = Triangle(vtxPosRasterSpace_0, vtxPosRasterSpace_1, vtxPosRasterSpace_2);

        // Check for trivial reject - no vertices in bin region
        if (!PrimitiveFoundInRegion(triangle, region))
        {
            continue;
        }

        const uint indexIntoBlock = orig & 31; // orig % 32
        const uint indexIntoBitmap = orig >> 5; // log_2(32) = 5

        InterlockedOr(smemPrimitiveBitmapBlocks[indexIntoBitmap], 1 << indexIntoBlock);
    }

    GroupMemoryBarrierWithGroupSync();

    const uint binOffset = (workgroupId.y * uLongLifeGlobals.ultraCoarseRasterDispatchSize.x) + workgroupId.x;
    const uint offsetToDensityBitmaps = binOffset * BIN_DATA_STRIDE;
    const uint offsetToPrimitiveBitmaps = offsetToDensityBitmaps + MAX_DENSITY_BITMAP_BLOCK_COUNT;
    PushConst.binningRasterizerBuffer[offsetToPrimitiveBitmaps + linearLocalThreadId] = smemPrimitiveBitmapBlocks[linearLocalThreadId];
}
