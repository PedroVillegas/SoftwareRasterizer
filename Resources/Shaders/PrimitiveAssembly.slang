// The Primitive Prep stage

#include "Shared.slangi"
#include "RasterizerUtility.slangi"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

static const Region2D GUARDBAND_CLIP_REGION_RASTER_SPACE = Region2D(-(1 << 14), 1 << 14);
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

    Triangle processed = ProcessPrimitive(vtxPosClipSpace_0, vtxPosClipSpace_1, vtxPosClipSpace_2);

    // PushConst.intrinsicDataBuffer[triangleBeginOffset].position = float4(vtxPosRasterSpace_0, 0.0, 0.0);
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = float4(vtxPosRasterSpace_1, 0.0, 0.0);
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = float4(vtxPosRasterSpace_2, 0.0, 0.0);

    PushConst.intrinsicDataBuffer[triangleBeginOffset].position = float4(processed.v0, 0.0, triangleBeginOffset);
    PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = float4(processed.v1, 0.0, 0.0);
    PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = float4(processed.v2, 0.0, 0.0);

    // GroupMemoryBarrierWithGroupSync();

    // w component of first will contain the total triangle count (triangle count + clipper emitted triangles)
    // Atomic into shared mem + atomic add into here??
    // if (globalThreadId.x == 0)
    // {
    //     PushConst.intrinsicDataBuffer[0].position.w = PushConst.triangleCount;
    // }
}

Triangle ProcessPrimitive(float4 vtxPosClipSpace_0, float4 vtxPosClipSpace_1, float4 vtxPosClipSpace_2)
{
    // Perspective divide (normalising projected view volume to canonical view / normalised device coordinates)
    // Canonical view volume / NDC: -1 <= X <= 1, -1 <= Y <= 1, 0 <= Z <= 1
    float4 vtxPosNdcSpace_0 = vtxPosClipSpace_0 / vtxPosClipSpace_0.w;
    float4 vtxPosNdcSpace_1 = vtxPosClipSpace_1 / vtxPosClipSpace_1.w;
    float4 vtxPosNdcSpace_2 = vtxPosClipSpace_2 / vtxPosClipSpace_2.w;

    // Backface cull AFTER perspective divide
    float3 normal = cross((vtxPosNdcSpace_1 - vtxPosNdcSpace_0).xyz, (vtxPosNdcSpace_2 - vtxPosNdcSpace_0).xyz);
    if (normal.z >= 0)
    {
        return Triangle(
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX)
        );
    }

    int2 halfFramebuffer = int2(uLongLifeGlobals.framebufferSize * float2(0.5));
    float2 guardBandRegionMinNdcSpace = float2(GUARDBAND_CLIP_REGION_RASTER_SPACE.min) / float2(halfFramebuffer);
    float2 guardBandRegionMaxNdcSpace = float2(GUARDBAND_CLIP_REGION_RASTER_SPACE.max) / float2(halfFramebuffer);

    // Clip against outer Guard-band perimeter

    if ( VertexNotInRegion(vtxPosNdcSpace_0.xy, guardBandRegionMinNdcSpace, guardBandRegionMaxNdcSpace) ||
         VertexNotInRegion(vtxPosNdcSpace_0.xy, guardBandRegionMinNdcSpace, guardBandRegionMaxNdcSpace) ||
         VertexNotInRegion(vtxPosNdcSpace_0.xy, guardBandRegionMinNdcSpace, guardBandRegionMaxNdcSpace) )
    {
        Triangle3D t = Triangle3D(vtxPosNdcSpace_0.xyz, vtxPosNdcSpace_1.xyz, vtxPosNdcSpace_2.xyz);
        Polygon clipped = SutherlandHodgman(t);
    }

    // for (uint32_t i = 0; i < emittedTriangles.size(); i++)
    // {
    //     emittedTriangles[i] = {
    //         .a = clipped[0],
    //         .b = clipped[i + 1],
    //         .c = clipped[i + 2],
    //     };
    // }

    // Debug:
    // PushConst.intrinsicDataBuffer[triangleBeginOffset].position = vtxPosNdcSpace_0;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = vtxPosNdcSpace_1;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = vtxPosNdcSpace_2;

    int2 vtxPosRasterSpace_0 = int2(vtxPosNdcSpace_0.xy * halfFramebuffer);
    int2 vtxPosRasterSpace_1 = int2(vtxPosNdcSpace_1.xy * halfFramebuffer);
    int2 vtxPosRasterSpace_2 = int2(vtxPosNdcSpace_2.xy * halfFramebuffer);

    Triangle triangle = Triangle(vtxPosRasterSpace_0, vtxPosRasterSpace_1, vtxPosRasterSpace_2);

    // TODO: Perform clipping in clip space BEFORE perspective divide and AFTER viewport culling.
    //       Will have to perform a buffer compaction after
    // Viewport culling
    Region2D framebufferRasterSpace = Region2D(-halfFramebuffer, uLongLifeGlobals.framebufferSize - halfFramebuffer);
    if (!PrimitiveFoundInRegion(triangle, framebufferRasterSpace) || triangle.SignedArea() == 0)
    {
        // vtxPosRasterSpace_0 = int2(CULLED_VERTEX);
        // vtxPosRasterSpace_1 = int2(CULLED_VERTEX);
        // vtxPosRasterSpace_2 = int2(CULLED_VERTEX);

        return Triangle(
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX)
        );
    };

    return triangle;
}

struct Triangle3D
{
    float3 a, b, c;
};

struct Plane
{
    float3 n;
    float d;

    float DistanceToPoint(float3 v)
    {
        return dot(n, v) - d;
    }

    float3 IntersectionWithLine(float3 a, float3 b)
    {
        float3 c = b - a;
        float d = DistanceToPoint(a);
        float t = -d / dot(n, c);
        return a + (c * t);
    }
};

struct Polygon
{
    float3 vertices[7];
    int vertexCount;

    [mutating]
    void AddVertex(float3 v)
    {
        vertices[vertexCount] = v;
        vertexCount++;
    }

    static Polygon MakeNew()
    {
        Polygon p = Polygon();
        p.vertexCount = 0;
        p.vertices[0] = float3(0);
        p.vertices[1] = float3(0);
        p.vertices[2] = float3(0);
        p.vertices[3] = float3(0);
        p.vertices[4] = float3(0);
        p.vertices[5] = float3(0);
        p.vertices[6] = float3(0);
        return p;
    }
};

static Polygon SutherlandHodgman(Triangle3D triangle)
{
    // Distances from plane
    float D1 = 0.0F, D2 = 0.0F;
    float EPSILON = 1.0E-6F;

    // The final polygon clipped against a frustum will have at most 7 vertices (5 triangles)
    Polygon p = Polygon::MakeNew();
    p.AddVertex(triangle.a);
    p.AddVertex(triangle.b);
    p.AddVertex(triangle.c);

    Plane planes[4] = {
        Plane(float3(-1.0, 0.0, 0.0), 1.0), // Left
        Plane(float3(1.0, 0.0, 0.0), 1.0),  // Right
        Plane(float3(0.0, 1.0, 0.0), 1.0),  // Top
        Plane(float3(0.0, -1.0, 0.0), 1.0), // Bottom
    };

    for (uint32_t d = 0; d < 4; ++d)
    {
        Polygon clipped = Polygon::MakeNew();
        for (uint32_t i = 0; i < p.vertexCount; i++)
        {
            // Clip against x = 1 plane;
            int next = (i + 1) % p.vertexCount;
            D1 = planes[d].DistanceToPoint(p.vertices[i]);
            D2 = planes[d].DistanceToPoint(p.vertices[next]);
            if ((D1 <= 0) && (D2 <= 0))
            {
                clipped.AddVertex(p.vertices[next]);
            }
            else if ((D1 > 0) && ((D2 > -EPSILON) && (D2 < EPSILON)))
            {
                clipped.AddVertex(p.vertices[next]);
            }
            else if (((D1 > -EPSILON) && (D1 < EPSILON)) && (D2 > 0))
            {
                continue;
            }
            else if ((D1 <= 0) && (D2 > 0))
            {
                clipped.AddVertex(planes[d].IntersectionWithLine(p.vertices[i], p.vertices[next]));
            }
            else if ((D1 > 0) && (D2 <= 0))
            {
                clipped.AddVertex(planes[d].IntersectionWithLine(p.vertices[i], p.vertices[next]));
                clipped.AddVertex(p.vertices[next]);
            }
        }
        p = clipped;
    }

    return p;
}

static bool VertexNotInRegion(float2 v, float2 rmin, float2 rmax)
{
    return any(v > rmax) || any(v < rmin);
}
