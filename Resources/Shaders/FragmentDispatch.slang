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
    uint2* fragShaderIndirectDispatchBuffer;
    uint triangleCount;
    uint framebufferWidth;
    uint framebufferHeight;
    uint depthImgId;
};

[[vk::push_constant]]
PC PushConst;

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    float fragmentCount = float(PushConst.fragShaderIndirectDispatchBuffer[1].y);

    // Fill the dispatch workgroup size needed
    PushConst.fragShaderIndirectDispatchBuffer[0].x = uint(floor(fragmentCount / 16.0F)) + 1;
    PushConst.fragShaderIndirectDispatchBuffer[0].y = 1;
    PushConst.fragShaderIndirectDispatchBuffer[1].x = 1;
}
