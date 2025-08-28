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
    GraphicsPipelineIntrinsicData* intrinsicDataBuffer;
    Vertex* vbuffer;
    float4x4 mvp;
    uint vertexCount;
};

[[vk::push_constant]]
PC PushConst;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= PushConst.vertexCount) return;

    Vertex v = PushConst.vbuffer[id];

    PushConst.intrinsicDataBuffer[id].homogeneousPosition = mul(PushConst.mvp, float4(v.position, 1.0));
}
