#define STORAGE_IMAGE_BINDING 0

[[vk::binding(STORAGE_IMAGE_BINDING, 0)]]
__DynamicResource uStorageTextures[];

struct GraphicsPipelineIntrinsicData
{
    float4 homogeneousPosition;
};

struct Vertex
{
    float3 position;
    float2 uv;
};

struct PC
{
    uint2* fragShaderIndirectDispatchBuffer;
    uint renderImgId;
};

[[vk::push_constant]]
PC PushConst;

[shader("compute")]
[numthreads(16, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= PushConst.fragShaderIndirectDispatchBuffer[1].y) return;

    uint2 pixel = PushConst.fragShaderIndirectDispatchBuffer[id + 2];

    RWTexture2D<float4> renderTexture = uStorageTextures[PushConst.renderImgId];
    renderTexture[pixel] = float4(1.0);
}
