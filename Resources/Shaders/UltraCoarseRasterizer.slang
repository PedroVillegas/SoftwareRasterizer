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
    const uint triangleCount = PushConst.metadataBuffer[0].compactTriangleBufferSize;
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
        uint orig;
        InterlockedAdd(smemTrianglesProcessed, 1, orig);

        if (orig >= triangleCount) break;

        // Fetch triangle
        TriangleIndexed triangleIndexed = PushConst.compactTriangleBuffer[orig];

        int2 vtxPosRasterSpace_0 = PushConst.homogeneousPositionBuffer[triangleIndexed.i0].position.xy;
        int2 vtxPosRasterSpace_1 = PushConst.homogeneousPositionBuffer[triangleIndexed.i1].position.xy;
        int2 vtxPosRasterSpace_2 = PushConst.homogeneousPositionBuffer[triangleIndexed.i2].position.xy;

        Triangle triangle = Triangle(vtxPosRasterSpace_0, vtxPosRasterSpace_1, vtxPosRasterSpace_2);

        // Check for trivial reject - no vertices in bin region
        if (!PrimitiveFoundInRegion(triangle, region))
        {
            // if (all(workgroupId.xy == uint2(0, 0)))
            //             printf("(Triangle %u)\nNOT IN TILE\n", orig);
            continue;
        }

        // triangleId = indexIntoBitmap * 32 + indexIntoBlock
        const uint indexIntoBlock = orig & 31; // orig % 32
        const uint indexIntoBitmap = orig >> 5; // log_2(32) = 5

        InterlockedOr(smemPrimitiveBitmapBlocks[indexIntoBitmap], 1 << indexIntoBlock);

        // if (all(workgroupId.xy == uint2(0, 0)))// && smemTrianglesProcessed < 32)
        // {
        //     printf("(Triangle %u)\nsmemPrimBlock = %u, bitmap = %u\n", orig, smemPrimitiveBitmapBlocks[indexIntoBitmap], indexIntoBitmap);
        // }
    }

    GroupMemoryBarrierWithGroupSync();

    const uint binOffset = (workgroupId.y * uLongLifeGlobals.ultraCoarseRasterDispatchSize.x) + workgroupId.x;
    const uint offsetToDensityBitmaps = binOffset * BIN_DATA_STRIDE;
    const uint offsetToPrimitiveBitmaps = offsetToDensityBitmaps + MAX_DENSITY_BITMAP_BLOCK_COUNT;
    PushConst.tileDataBuffer[offsetToPrimitiveBitmaps + linearLocalThreadId] = smemPrimitiveBitmapBlocks[linearLocalThreadId];
}
