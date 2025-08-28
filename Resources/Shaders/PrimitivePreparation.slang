// The Primitive Prep stage

#include "Shared.slangi"
#include "RasterizerUtility.slangi"

struct PC
{
    uint* binningRasterizerBuffer;
    GraphicsPipelineIntrinsicData* intrinsicDataBuffer;
    uint triangleCount;
    uint renderImgId;
};

[[vk::push_constant]]
PC PushConst;

static const Region2D GUARDBAND_CLIP_REGION = Region2D(-(1 << 14), 1 << 14);
// static const Region2D GUARDBAND_CLIP_REGION = Region2D(int2(-200, -100), int2(200, 100));

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID,
          uint3 workgroupId : SV_GroupID,
          uint3 localThreadId : SV_GroupThreadID)
{
    if (globalThreadId.x >= PushConst.triangleCount) return;

    uint triangleBeginOffset = globalThreadId.x * 3;

    // Fetch triangle
    float4 vtxPosClipSpace_0 = PushConst.intrinsicDataBuffer[triangleBeginOffset].position;
    float4 vtxPosClipSpace_1 = PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position;
    float4 vtxPosClipSpace_2 = PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position;

    // Clip against outer Guard-band perimeter

    // Perspective divide (normalising projected view volume to canonical view / normalised device coordinates)
    // Canonical view volume / NDC: -1 <= X <= 1, -1 <= Y <= 1, 0 <= Z <= 1
    float4 vtxPosNdcSpace_0 = vtxPosClipSpace_0 / vtxPosClipSpace_0.w;
    float4 vtxPosNdcSpace_1 = vtxPosClipSpace_1 / vtxPosClipSpace_1.w;
    float4 vtxPosNdcSpace_2 = vtxPosClipSpace_2 / vtxPosClipSpace_2.w;

    // Debug:
    // PushConst.intrinsicDataBuffer[triangleBeginOffset].position = vtxPosNdcSpace_0;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = vtxPosNdcSpace_1;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = vtxPosNdcSpace_2;

    int2 halfFramebuffer = int2(uLongLifeGlobals.framebufferSize * float2(0.5));
    int2 vtxPosRasterSpace_0 = int2(vtxPosNdcSpace_0.xy * halfFramebuffer);
    int2 vtxPosRasterSpace_1 = int2(vtxPosNdcSpace_1.xy * halfFramebuffer);
    int2 vtxPosRasterSpace_2 = int2(vtxPosNdcSpace_2.xy * halfFramebuffer);

    Triangle triangle = Triangle(vtxPosRasterSpace_0, vtxPosRasterSpace_1, vtxPosRasterSpace_2);

    // Viewport culling
    Region2D framebufferRasterSpace = Region2D(-halfFramebuffer, uLongLifeGlobals.framebufferSize - halfFramebuffer);
    if (!PrimitiveFoundInRegion(triangle, framebufferRasterSpace) || triangle.SignedArea() == 0)
    {
        vtxPosRasterSpace_0 = int2(CULLED_VERTEX);
        vtxPosRasterSpace_1 = int2(CULLED_VERTEX);
        vtxPosRasterSpace_2 = int2(CULLED_VERTEX);
    };

    PushConst.intrinsicDataBuffer[triangleBeginOffset].position = float4(vtxPosRasterSpace_0, 0.0, 0.0);
    PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = float4(vtxPosRasterSpace_1, 0.0, 0.0);
    PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = float4(vtxPosRasterSpace_2, 0.0, 0.0);

    GroupMemoryBarrierWithGroupSync();

    // w component of first will contain the total triangle count (triangle count + clipper emitted triangles)
    if (globalThreadId.x == 0)
    {
        PushConst.intrinsicDataBuffer[0].position.w = PushConst.triangleCount;
    }
}
