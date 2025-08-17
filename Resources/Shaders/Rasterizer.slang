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
    uint2* fragShaderIndirectDispatchBuffer;
    uint triangleCount;
    uint framebufferWidth;
    uint framebufferHeight;
    uint depthImgId;
};

[[vk::push_constant]]
PC PushConst;

[shader("compute")]
[numthreads(16, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= PushConst.triangleCount) return;

    // Fetch homogenous vertex positions for triangle to rasterize
    uint triangleBeginOffset = id * 3;
    float4 hvp_0 = PushConst.intrinsicDataBuffer[triangleBeginOffset].homogeneousPosition;
    float4 hvp_1 = PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].homogeneousPosition;
    float4 hvp_2 = PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].homogeneousPosition;

    // Perspective divide (normalising projected view volume to canonical view  / normalised device coordinates)
    // Canonical view volume / NDC: -1 <= X <= 1, -1 <= Y <= 1, 0 <= Z <= 1
    // ndcvp = normalised device coordinate vertex position
    float4 ndcvp_0 = hvp_0 / hvp_0.w;
    float4 ndcvp_1 = hvp_1 / hvp_1.w;
    float4 ndcvp_2 = hvp_2 / hvp_2.w;

    // Debug:
    // PushConst.intrinsicDataBuffer[triangleBeginOffset].homogeneousPosition = ndcvp_0;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].homogeneousPosition = ndcvp_1;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].homogeneousPosition = ndcvp_2;

    // Push framebuffer coordinates to indirect dispatch buffer which will be used in fragment shader
    uint2 fbcvp_0 = uint2(((ndcvp_0.xy + float2(1.0)) * float2(0.5)) * float2(PushConst.framebufferWidth, PushConst.framebufferHeight));
    uint2 fbcvp_1 = uint2(((ndcvp_1.xy + float2(1.0)) * float2(0.5)) * float2(PushConst.framebufferWidth, PushConst.framebufferHeight));
    uint2 fbcvp_2 = uint2(((ndcvp_2.xy + float2(1.0)) * float2(0.5)) * float2(PushConst.framebufferWidth, PushConst.framebufferHeight));

    // Debug:
    // PushConst.intrinsicDataBuffer[triangleBeginOffset].homogeneousPosition = float4(fbcvp_0, 0.0, 0.0);
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].homogeneousPosition = float4(fbcvp_1, 0.0, 0.0);
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].homogeneousPosition = float4(fbcvp_2, 0.0, 0.0);

    uint2 bboxMin = min(fbcvp_0, min(fbcvp_1, fbcvp_2));
    uint2 bboxMax = max(fbcvp_0, max(fbcvp_1, fbcvp_2));

    // Debug:
    // PushConst.fragShaderIndirectDispatchBuffer[2 + id * 2] = bboxMin;
    // PushConst.fragShaderIndirectDispatchBuffer[3 + id * 2] = bboxMax;

    // return;

    uint fragmentInvocations = 0;

    // Check if each pixel in the bbox is inside the triangle
    for (uint x = bboxMin.x; x < bboxMax.x; ++x)
    {
        for (uint y = bboxMin.y; y < bboxMax.y; ++y)
        {
            uint2 sample = uint2(x, y);
            int firstEdgeTest = EdgeFn(fbcvp_0, fbcvp_1, sample);
            int secondEdgeTest = EdgeFn(fbcvp_1, fbcvp_2, sample);
            int thirdEdgeTest = EdgeFn(fbcvp_2, fbcvp_0, sample);

            if (firstEdgeTest >= 0 && secondEdgeTest >= 0 && thirdEdgeTest >= 0)
            {
                // Pixel sample is indeed inside the triangle
                // Push it into fragShaderIndirectDispatchBuffer as a new fragment invocation

                // uint currentOffset = PushConst.fragShaderIndirectDispatchBuffer[1].y;
                uint currentOffset = 0;
                InterlockedAdd(PushConst.fragShaderIndirectDispatchBuffer[1].y, 1, currentOffset);
                // Probably faster to load PushConst.fragShaderIndirectDispatchBuffer[1] into Groupshared Mem / LDS,
                // perform atomic operations on the shared mem, then write to it at the end with the shared mem value
                if (currentOffset >= 480000) return;
                PushConst.fragShaderIndirectDispatchBuffer[2 + currentOffset] = sample;
                // PushConst.fragShaderIndirectDispatchBuffer[1].y += 1;
                fragmentInvocations++;
            }
        }
    }
    //InterlockedAdd(PushConst.fragShaderIndirectDispatchBuffer[1].y, 1, fragmentInvocations);
    // PushConst.fragShaderIndirectDispatchBuffer[1].y += fragmentInvocations;
}

int EdgeFn(uint2 startVertex, uint2 endVertex, uint2 sample)
{
    int dx = endVertex.x - startVertex.x;
    int dy = endVertex.y - startVertex.y;

    return (sample.x - startVertex.x) * dy - (sample.y - startVertex.y) * dx;
}
