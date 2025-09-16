#include "Shared.slang"
#include "StreamCompaction.slang"

struct PredEvalFn : StreamCompactionPredicateEvaluationFn
{
    typealias PRED_TYPE = uint;
    typealias BUFFER_ELEM_TYPE = TriangleIndexed;

    bool EvaluatePredicate(const PRED_TYPE item)
    {
        return item != CULLED_VERTEX;
    }

    PRED_TYPE GetPredicateElement(BUFFER_ELEM_TYPE* src, const uint index)
    {
        return src[index].i0;
    }
}

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    PredEvalFn pef;
    StreamCompactionNonOrderPreserving<TriangleIndexed, PredEvalFn>(
            id,
            PushConst.sparseTriangleBuffer,
            PushConst.compactTriangleBuffer,
            uLongLifeGlobals.clipperEmittedVerticesOffset,
            PushConst.metadataBuffer[0].clipperEmittedTrianglesCount,
            PushConst.metadataBuffer[0].compactTriangleBufferSize,
            pef
    );
}
