#pragma DO_NOT_COMPILE

#ifndef _SHARED_H_
#define _SHARED_H_

#define STORAGE_IMAGE_BINDING 0
#define UNIFORM_BUFFER_BINDING 4

[[vk::binding(STORAGE_IMAGE_BINDING, 0)]]
__DynamicResource uStorageTextures[];

struct HomogeneousPosition
{
    float4 position;
};

struct GraphicsPipelineMetadata
{
    uint compactTriangleBufferSize;
    uint clipperEmittedTrianglesCount;
};

struct GraphicsPipelinePushConstants
{
    uint* tileDataBuffer;
    uint* indexBuffer;
    HomogeneousPosition* homogeneousPositionBuffer;
    TriangleIndexed* sparseTriangleBuffer;
    TriangleIndexed* compactTriangleBuffer;
    GraphicsPipelineMetadata* metadataBuffer;
    uint triangleCount;
    uint verticesCount;
    uint renderImgId;
};

struct LongLifetimeGlobalVars
{
    uint2 framebufferSize;
    uint32_t ultraCoarseRasterTileSize;
    uint32_t coarseRasterTileSize;
    uint32_t fineRasterTileSize;
    uint2 ultraCoarseRasterDispatchSize;
    uint2 coarseRasterDispatchSize;
    uint2 fineRasterDispatchSize;

    uint clipperEmittedVerticesOffset;
};

[[vk::binding(UNIFORM_BUFFER_BINDING, 0)]]
ConstantBuffer<LongLifetimeGlobalVars> uLongLifeGlobals;

// This should be a power of 2
static const uint MAX_PRIMITIVE_BITMAP_BLOCK_COUNT = 1024;

static const uint MAX_DENSITY_BITMAP_BLOCK_COUNT = 1;
static const uint BIN_DATA_STRIDE = MAX_DENSITY_BITMAP_BLOCK_COUNT * (1 + MAX_PRIMITIVE_BITMAP_BLOCK_COUNT);

static const uint CULLED_VERTEX = 0xFFFFFFFF; // Max uint32_t

struct Vertex
{
    float3 position;
    float2 uv;
};

struct Region2D
{
    int2 min;
    int2 max;
};

struct TriangleIndexed
{
    uint i0, i1, i2;
};

struct Triangle
{
    int2 v0;
    int2 v1;
    int2 v2;

    int SignedArea()
    {
        int dx = v0.x - v1.x;
        int dy = v0.y - v1.y;

        return (v2.x - v1.x) * dy - (v2.y - v1.y) * dx;
    }
};

struct Line
{
    int2 start;
    int2 end;
};

#endif // _SHARED_H
