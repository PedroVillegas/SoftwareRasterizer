struct HomogeneousPosition
{
    float4 position;
};

struct Vertex
{
    float3 position;
    float2 uv;
};

struct PC
{
    float4x4 mvp;
    HomogeneousPosition* homogeneousPositionBuffer;
    Vertex* vertexBuffer;
    uint* indexBuffer;
    uint vertexCount;
};

[[vk::push_constant]]
PC PushConst;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= PushConst.vertexCount) return;

    Vertex v = PushConst.vertexBuffer[id];

    PushConst.homogeneousPositionBuffer[id].position = mul(PushConst.mvp, float4(v.position, 1.0));
}
