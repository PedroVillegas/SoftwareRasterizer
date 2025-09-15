// This shader populates the Density Bitmaps found in the first
// `densityBitmapBlockCount` uint32_t's at the beginning of each bin's
// data block in `GSR::binningRasterBuffer`.
//
// Each block of the Density Bitmap is a uint32_t such that the nth bit
// denotes whether the nth block of the bin's Primitive Bitmap has any
// bits set. If the block is empty, then the bit is set to 0, and 1 otherwise.
//
// Each thread maps 32 contiguous Primitive Bitmap blocks to a single
// Density Bitmap block.

#include "Shared.slang"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

groupshared uint[MAX_DENSITY_BITMAP_BLOCK_COUNT] smemDensityBitmap;

[shader("compute")]
[numthreads(32, 1, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID,
          uint3 workgroupId : SV_GroupID,
          uint3 localThreadId : SV_GroupThreadID)
{
    if (localThreadId.x >= 32) return;

    const uint binOffset = workgroupId.x;
    const uint offsetToDensityBitmaps = binOffset * BIN_DATA_STRIDE;
    const uint offsetToPrimitiveBitmaps = offsetToDensityBitmaps + MAX_DENSITY_BITMAP_BLOCK_COUNT;

    if (localThreadId.x < MAX_DENSITY_BITMAP_BLOCK_COUNT)
    {
        smemDensityBitmap[localThreadId.x] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint i = 0; i < MAX_DENSITY_BITMAP_BLOCK_COUNT; ++i)
    {
        for (uint j = 0; j < 32; ++j)
        {
            const uint offset = offsetToPrimitiveBitmaps + (i << 10) + (j << 5) + localThreadId.x;
            const uint primitiveBitmapBlock = PushConst.binningRasterizerBuffer[offset];

            if (primitiveBitmapBlock != 0)
            {
                smemDensityBitmap[i] |= 1 << j;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (localThreadId.x < MAX_DENSITY_BITMAP_BLOCK_COUNT)
    {
        PushConst.binningRasterizerBuffer[offsetToDensityBitmaps + localThreadId.x] = smemDensityBitmap[localThreadId.x];
    }
}
