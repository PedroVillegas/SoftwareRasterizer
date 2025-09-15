#include "Shared.slang"

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
    const uint2 mappedWorkgroupId = workgroupId.xy / WorkgroupSize().xy;
    const uint binOffset = (mappedWorkgroupId.y * uLongLifeGlobals.ultraCoarseRasterDispatchSize.x) + mappedWorkgroupId.x;
    const uint offsetToDensityBitmaps = binOffset * BIN_DATA_STRIDE;
    const uint offsetToPrimitiveBitmaps = offsetToDensityBitmaps + MAX_DENSITY_BITMAP_BLOCK_COUNT;

    const uint linearLocalThreadId = (localThreadId.y * WorkgroupSize().x) + localThreadId.x;
    if (linearLocalThreadId < MAX_DENSITY_BITMAP_BLOCK_COUNT)
    {
        smemDensityBitmapBlocks[linearLocalThreadId] = PushConst.binningRasterizerBuffer[offsetToDensityBitmaps + linearLocalThreadId];
    }

    GroupMemoryBarrierWithGroupSync();

    // if (all(mappedWorkgroupId == uint2(0, 0)) && linearLocalThreadId < 1)
    // {
    //     printf("(Thread-%u) PrimitiveBitmapBlock = %u\n", linearLocalThreadId, PushConst.binningRasterizerBuffer[offsetToPrimitiveBitmaps + linearLocalThreadId]);
    // }

#if 0
    for (uint densityBitmapBlockIndex = 0; densityBitmapBlockIndex < MAX_DENSITY_BITMAP_BLOCK_COUNT; densityBitmapBlockIndex++)
    {
        uint densityBitmapBlock = smemDensityBitmapBlocks[densityBitmapBlockIndex];
        uint i = countbits(densityBitmapBlock);
    }
#elif
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
                uint primBitmapBitCount = countbits(primBitmap);
                for (uint primitiveBitmapBlockBit = 0; primitiveBitmapBlockBit < 32; ++primitiveBitmapBlockBit)
                {
                    const uint pbbBit = 1 << primitiveBitmapBlockBit;
                    if ((primBitmap & pbbBit) == 0) continue;

                    const uint triangleId = (densityBitmapBlockIndex << 10) + (densityBitmapBlockBit << 5) + (primitiveBitmapBlockIndex << 5) + primitiveBitmapBlockBit;
                    const uint firstVertex = 3 * triangleId;

                    int2 vtxPosRasterSpace_0 = PushConst.compactedIntrinsicDataBuffer[firstVertex].position.xy;
                    int2 vtxPosRasterSpace_1 = PushConst.compactedIntrinsicDataBuffer[firstVertex + 1].position.xy;
                    int2 vtxPosRasterSpace_2 = PushConst.compactedIntrinsicDataBuffer[firstVertex + 2].position.xy;

                    // if (all(mappedWorkgroupId == uint2(0, 0)) && linearLocalThreadId < 1 && firstVertex == 848)
                    // {
                    //     printf("(Thread-%u)\nprimBitmapBitCount = %u\nfirstVertex = %u\ndensityBitmapBlockBit = %u\nprimBitmapBlockBit = %u\n",
                    //     linearLocalThreadId, primBitmapBitCount, firstVertex, densityBitmapBlockBit, primitiveBitmapBlockBit);
                    // }

                    int2 sampleRasterSpace = globalThreadId.xy - halfFramebuffer;
                    int firstEdgeTest = EdgeFn(vtxPosRasterSpace_0, vtxPosRasterSpace_1, sampleRasterSpace);
                    int secondEdgeTest = EdgeFn(vtxPosRasterSpace_1, vtxPosRasterSpace_2, sampleRasterSpace);
                    int thirdEdgeTest = EdgeFn(vtxPosRasterSpace_2, vtxPosRasterSpace_0, sampleRasterSpace);

                    if ( (firstEdgeTest >= 0 && secondEdgeTest >= 0 && thirdEdgeTest >= 0)
                      || (firstEdgeTest < 0 && secondEdgeTest < 0 && thirdEdgeTest < 0)
                    )
                    {
                        renderTexture[globalThreadId.xy] = float4(FillPixel(PushConst.compactedIntrinsicDataBuffer[firstVertex].position.w), 1.0);
                    }

                    // if (all(mappedWorkgroupId == uint2(0, 0)))
                    // {
                    //     // DEBUG:
                    //     renderTexture[uint2(10)] = float4(0.0, 0.0, 0.0, 1.0);
                    //     renderTexture[uint2(10, 11)] = float4(0.0, 0.0, 0.0, 1.0);
                    //     renderTexture[uint2(11, 10)] = float4(0.0, 0.0, 0.0, 1.0);
                    //     renderTexture[uint2(11)] = float4(0.0, 0.0, 0.0, 1.0);
                    // }
                }
            }
        }
    }

    // if (all(mappedWorkgroupId == uint2(0, 0)) && linearLocalThreadId < 1)
    // {
    //     printf("(Thread-%u) TrianglesTested = %u\n", linearLocalThreadId, trianglesTested);
    // }
#endif
}

float3 FillPixel(uint id)
{
    uint hashed = hash(id);
    float3 o = float3(float(hashed & 255U), float((hashed >> 8) & 255U), float((hashed >> 16) & 255U)) / 255.0;
    return pow(o, float3(1.0 / 2.2));

    // o.x = o.x * 0.4 + 0.6;
    // o.y = o.y * 0.1 + 0.05;
    // o.z = o.z * 0.05 + 0.04;

    // Vary lightness [0.0, 1.0] and chroma [0.0, 0.4]
    o.x = o.x * 0.4 + 0.58;
    o.y = o.y * 0.08 + 0.1;
    // Keep hue constant [0.0, 360.0)
    o.z = 10.0;

    float3 c = OklabToSrgb(OklchToOklab(o));
    return pow(c, float3(1.0 / 2.2));
    return c;
    // return float3(float(hashed & 255U), float((hashed >> 8) & 255U), float((hashed >> 16) & 255U)) / 255.0;
}

int EdgeFn(int2 startVertex, int2 endVertex, int2 sample)
{
    int dx = endVertex.x - startVertex.x;
    int dy = endVertex.y - startVertex.y;

    return (sample.x - startVertex.x) * dy - (sample.y - startVertex.y) * dx;
}

uint hash(uint a)
{
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^uint(0xc761c23c)) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^uint(0xb55a4f09)) ^ (a>>16);
    return a;
}

float3 OklabToSrgb(float3 c)
{
    c.x = saturate(c.x);         // L
    c.y = clamp(c.y, -0.5, 0.5); // a
    c.z = clamp(c.z, -0.5, 0.5); // b

    float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
    float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
    float s_ = c.x - 0.0894841775f * c.y - 1.2914855480f * c.z;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    return float3(
        +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s,
    );
}

float3 OklchToOklab(float3 c)
{
    const float hRad = radians(c.z); // convert hue from degrees to radians
    return float3(
        c.x,
        c.y * cos(hRad),
        c.y * sin(hRad)
    );
}

