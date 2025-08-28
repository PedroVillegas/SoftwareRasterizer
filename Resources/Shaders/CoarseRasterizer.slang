#define STORAGE_IMAGE_BINDING 0

[[vk::binding(STORAGE_IMAGE_BINDING, 0)]]
__DynamicResource uStorageTextures[];

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

    if (id.x >= renderDims.x || id.y >= renderDims.y) return;

    float2 uv = float2(id) / renderDims;

    float3 c = float3(uv, 0.5);

    renderTexture[id] = float4(pow(c, float3(1.0 / 2.2)), 1.0);

    RWTexture2D<float> depthTexture = uStorageTextures[PushConst.depthImgId];
    depthTexture[id] = 1.0;
}
