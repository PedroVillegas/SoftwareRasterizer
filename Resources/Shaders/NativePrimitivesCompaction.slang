#include "Shared.slang"
#include "StreamCompaction.slang"

struct PredEvalFn : StreamCompactionPredicateEvaluationFn
{
    typealias PRED_TYPE = float;
    typealias BUFFER_ELEM_TYPE = GraphicsPipelineIntrinsicData;

    bool EvaluatePredicate(const PRED_TYPE item)
    {
        return item != float(CULLED_VERTEX);
    }

    PRED_TYPE GetPredicateElement(BUFFER_ELEM_TYPE* src, const uint index)
    {
        return src[index].position.x;
    }
}

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

[shader("compute")]
[numthreads(1024, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    PredEvalFn pef;
    StreamCompactionNonOrderPreserving<GraphicsPipelineIntrinsicData, PredEvalFn>(
            id,
            PushConst.intrinsicDataBuffer,
            PushConst.compactedIntrinsicDataBuffer,
            0,
            PushConst.verticesCount,
            PushConst.metaDataBuffer[0].compactedIntrinsicDataBufferSize,
            pef
    );
}
