#include "Shared.slang"

struct PC
{
    uint renderImgId;
    uint depthImgId;
};

[[vk::push_constant]]
PC PushConst;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    RWTexture2D<float4> renderTexture = uStorageTextures[PushConst.renderImgId];

    uint2 renderDims;
    renderTexture.GetDimensions(renderDims.x, renderDims.y);

    if (any(id >= renderDims)) return;

    float2 uv = float2(id) / renderDims;

    float3 c = float3(uv, 0.5);

    renderTexture[id] = float4(1.0, 1.0, 1.0, 1.0);

    //if (id.x > 0 && (id.x & (uLongLifeGlobals.ultraCoarseRasterTileSize - 1)) == 0)
    //{
    //    renderTexture[id] = float4(0.0, 0.0, 0.0, 1.0);
    //}
//
    //if (id.y > 0 && (id.y & (uLongLifeGlobals.ultraCoarseRasterTileSize - 1)) == 0)
    //{
    //    renderTexture[id] = float4(0.0, 0.0, 0.0, 1.0);
    //}

    RWTexture2D<float> depthTexture = uStorageTextures[PushConst.depthImgId];
    depthTexture[id] = 1.0;
}
