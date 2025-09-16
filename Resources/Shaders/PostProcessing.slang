#include "Shared.slang"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    RWTexture2D<float4> renderTexture = uStorageTextures[PushConst.renderImgId];

    uint2 renderDims;
    renderTexture.GetDimensions(renderDims.x, renderDims.y);

    if (any(id >= renderDims)) return;

    if (id.x > 0 && (id.x & (uLongLifeGlobals.ultraCoarseRasterTileSize - 1)) == 0)
    {
        renderTexture[id] = float4(0.0, 0.0, 0.0, 1.0);
    }

    if (id.y > 0 && (id.y & (uLongLifeGlobals.ultraCoarseRasterTileSize - 1)) == 0)
    {
        renderTexture[id] = float4(0.0, 0.0, 0.0, 1.0);
    }
}
