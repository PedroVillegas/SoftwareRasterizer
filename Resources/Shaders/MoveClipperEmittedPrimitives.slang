// Moves clipper emitted primitives to the end of the compacted intrinsics data buffer

#include "Shared.slangi"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= PushConst.metaDataBuffer[0].clipperEmittedVerticesCount) return;

    uint offset = PushConst.metaDataBuffer[0].compactedIntrinsicDataBufferSize + id;
    GraphicsPipelineIntrinsicData data = PushConst.intrinsicDataBuffer[uLongLifeGlobals.clipperEmittedVerticesOffset + id];

    // if (data.position.x == CULLED_VERTEX) return;

    PushConst.compactedIntrinsicDataBuffer[offset] = data;
    GraphicsPipelineIntrinsicData clear;
    clear.position = float4(0.0);
    PushConst.intrinsicDataBuffer[uLongLifeGlobals.clipperEmittedVerticesOffset + id] = clear;

    InterlockedAdd(PushConst.metaDataBuffer[0].validClipperEmittedTrianglesCount, 1);
}
