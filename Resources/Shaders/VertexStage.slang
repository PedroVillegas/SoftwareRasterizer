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
    float4x4 mvp;
    GraphicsPipelineIntrinsicData* intrinsicDataBuffer;
    Vertex* vertexBuffer;
    uint* indexBuffer;
    uint indexCount;
};

[[vk::push_constant]]
PC PushConst;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= PushConst.indexCount) return;

    Vertex v = PushConst.vertexBuffer[PushConst.indexBuffer[id]];

    PushConst.intrinsicDataBuffer[id].homogeneousPosition = mul(PushConst.mvp, float4(v.position, 1.0));
}
