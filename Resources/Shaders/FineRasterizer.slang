#include "Shared.slangi"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

groupshared uint[MAX_DENSITY_BITMAP_BLOCK_COUNT] smemDensityBitmapBlocks;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID,
          uint3 workgroupId : SV_GroupID,
          uint3 localThreadId : SV_GroupThreadID)
{
    RWTexture2D<float4> renderTexture = uStorageTextures[PushConst.renderImgId];

    uint2 renderDims;
    renderTexture.GetDimensions(renderDims.x, renderDims.y);

    if (globalThreadId.x >= renderDims.x || globalThreadId.y >= renderDims.y) return;

    int2 halfFramebuffer = int2(uLongLifeGlobals.framebufferSize * float2(0.5));

    // Find out what bin this tile is a part of and its bounds
    const uint2 mappedWorkgroupId = workgroupId.xy / uint2(16);
    const uint binOffset = (mappedWorkgroupId.y * 4) + mappedWorkgroupId.x;
    const uint offsetToDensityBitmaps = binOffset * BIN_DATA_STRIDE;
    const uint offsetToPrimitiveBitmaps = offsetToDensityBitmaps + MAX_DENSITY_BITMAP_BLOCK_COUNT;

    const uint linearLocalThreadId = (localThreadId.y * WorkgroupSize().x) + localThreadId.x;
    if (linearLocalThreadId < MAX_DENSITY_BITMAP_BLOCK_COUNT)
    {
        smemDensityBitmapBlocks[linearLocalThreadId] = PushConst.binningRasterizerBuffer[offsetToDensityBitmaps + linearLocalThreadId];
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint densityBitmapBlockIndex = 0; densityBitmapBlockIndex < MAX_DENSITY_BITMAP_BLOCK_COUNT; ++densityBitmapBlockIndex)
    {
        uint densityBitmapBlock = smemDensityBitmapBlocks[densityBitmapBlockIndex];
        if (densityBitmapBlock == 0) continue;

        // Iterate over each bit of densityBitmapBlock
        for (uint densityBitmapBlockBit = 0; densityBitmapBlockBit < 32; ++densityBitmapBlockBit)
        {
            const uint bit = 1 << densityBitmapBlockBit;
            if ((densityBitmapBlock & bit) == 0) continue;

            // Iterate over the corresponding 32 Primitive Bitmap blocks
            for (uint primitiveBitmapBlockIndex = 0; primitiveBitmapBlockIndex < 32; ++primitiveBitmapBlockIndex)
            {
                uint primBitmap = PushConst.binningRasterizerBuffer[offsetToPrimitiveBitmaps + primitiveBitmapBlockIndex];
                // Iterate over each bit of primBitmap
                for (uint primitiveBitmapBlockBit = 0; primitiveBitmapBlockBit < 32; ++primitiveBitmapBlockBit)
                {
                    const uint pbbBit = 1 << primitiveBitmapBlockBit;
                    if ((primBitmap & pbbBit) == 0) continue;

                    const uint triangleBeginIndex = 3 * ((densityBitmapBlockBit << 5) + primitiveBitmapBlockBit);
                    int2 vtxPosRasterSpace_0 = PushConst.compactedIntrinsicDataBuffer[triangleBeginIndex].position.xy;
                    int2 vtxPosRasterSpace_1 = PushConst.compactedIntrinsicDataBuffer[triangleBeginIndex + 1].position.xy;
                    int2 vtxPosRasterSpace_2 = PushConst.compactedIntrinsicDataBuffer[triangleBeginIndex + 2].position.xy;

                    int2 sampleRasterSpace = globalThreadId.xy - halfFramebuffer;
                    int firstEdgeTest = EdgeFn(vtxPosRasterSpace_0, vtxPosRasterSpace_1, sampleRasterSpace);
                    int secondEdgeTest = EdgeFn(vtxPosRasterSpace_1, vtxPosRasterSpace_2, sampleRasterSpace);
                    int thirdEdgeTest = EdgeFn(vtxPosRasterSpace_2, vtxPosRasterSpace_0, sampleRasterSpace);

                    // DEBUG:
                    // renderTexture[uint2(10)] = float4(1.0);
                    // renderTexture[uint2(10, 11)] = float4(1.0);
                    // renderTexture[uint2(11, 10)] = float4(1.0);
                    // renderTexture[uint2(11)] = float4(1.0);

                    if ( (firstEdgeTest >= 0 && secondEdgeTest >= 0 && thirdEdgeTest >= 0)
                      || (firstEdgeTest < 0 && secondEdgeTest < 0 && thirdEdgeTest < 0)
                    )
                    {
                        renderTexture[globalThreadId.xy] = float4(FillPixel(PushConst.compactedIntrinsicDataBuffer[triangleBeginIndex].position.w), 1.0);
                    }
                }
            }
        }
    }
}

float3 FillPixel(uint id)
{
    //return float3(1.0);
    uint hashed = hash(id);
    return float3(float(hashed & 255U), float((hashed >> 8) & 255U), float((hashed >> 16) & 255U)) / 255.0;
}

int EdgeFn(int2 startVertex, int2 endVertex, int2 sample)
{
    int dx = endVertex.x - startVertex.x;
    int dy = endVertex.y - startVertex.y;

    return (sample.x - startVertex.x) * dy - (sample.y - startVertex.y) * dx;
}

uint hash(uint a)
{
    a = (a+0x7edf5d16) + (a<<12);
    a = (a^uint(0xc161c23c)) ^ (a>>19);
    a = (a+0x1c2667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^uint(0xb55a4f01)) ^ (a>>16);
    return a;
}
