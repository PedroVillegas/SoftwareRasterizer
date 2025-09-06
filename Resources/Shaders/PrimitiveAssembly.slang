// The Primitive Prep stage

#include "Shared.slangi"
#include "RasterizerUtility.slangi"

[[vk::push_constant]]
GraphicsPipelinePushConstants PushConst;

#define BACKFACE_CULLING 1

static const float NEAR_PLANE = 0.01;
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

    // Viewport Culling before clipping
    if (
        (vtxPosClipSpace_0.x < -vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.x < -vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.x < -vtxPosClipSpace_2.w) ||

        (vtxPosClipSpace_0.x > vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.x > vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.x > vtxPosClipSpace_2.w) ||

        (vtxPosClipSpace_0.y < -vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.y < -vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.y < -vtxPosClipSpace_2.w) ||

        (vtxPosClipSpace_0.y > vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.y > vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.y > vtxPosClipSpace_2.w) ||

        (vtxPosClipSpace_0.z < NEAR_PLANE &&
         vtxPosClipSpace_1.z < NEAR_PLANE &&
         vtxPosClipSpace_2.z < NEAR_PLANE) ||

        (vtxPosClipSpace_0.z > vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.z > vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.z > vtxPosClipSpace_2.w)
    )
    {
        // printf("TRIVIAL REJECT\n");
        PushConst.intrinsicDataBuffer[triangleBeginOffset].position = float4(float2(CULLED_VERTEX), 0.0, triangleBeginOffset);
        PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = float4(float2(CULLED_VERTEX), 0.0, 0.0);
        PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = float4(float2(CULLED_VERTEX), 0.0, 0.0);
        return;
    }

    // Completely inside clipping volume
    if (
        (vtxPosClipSpace_0.x >= -vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.x >= -vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.x >= -vtxPosClipSpace_2.w) &&

        (vtxPosClipSpace_0.x <= vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.x <= vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.x <= vtxPosClipSpace_2.w) &&

        (vtxPosClipSpace_0.y >= -vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.y >= -vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.y >= -vtxPosClipSpace_2.w) &&

        (vtxPosClipSpace_0.y <= vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.y <= vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.y <= vtxPosClipSpace_2.w) &&

        (vtxPosClipSpace_0.z >= NEAR_PLANE &&
         vtxPosClipSpace_1.z >= NEAR_PLANE &&
         vtxPosClipSpace_2.z >= NEAR_PLANE) &&

        (vtxPosClipSpace_0.z <= vtxPosClipSpace_0.w &&
         vtxPosClipSpace_1.z <= vtxPosClipSpace_1.w &&
         vtxPosClipSpace_2.z <= vtxPosClipSpace_2.w)
    )
    {
        // printf("TRIVIAL ACCEPT\n");
        Triangle processed = ProcessPrimitive(vtxPosClipSpace_0, vtxPosClipSpace_1, vtxPosClipSpace_2);

        // PushConst.intrinsicDataBuffer[triangleBeginOffset].position = float4(vtxPosRasterSpace_0, 0.0, 0.0);
        // PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = float4(vtxPosRasterSpace_1, 0.0, 0.0);
        // PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = float4(vtxPosRasterSpace_2, 0.0, 0.0);

        PushConst.intrinsicDataBuffer[triangleBeginOffset].position = float4(processed.v0, 0.0, triangleBeginOffset);
        PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = float4(processed.v1, 0.0, 0.0);
        PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = float4(processed.v2, 0.0, 0.0);
        return;
    }

    // Clip before homogeneous divide!
    Polygon clipped = SutherlandHodgman(
        Triangle3D(
            vtxPosClipSpace_0,
            vtxPosClipSpace_1,
            vtxPosClipSpace_2,
        )
    );

    // if (globalThreadId.x == 0) printf("clipped\n");

    PushConst.intrinsicDataBuffer[triangleBeginOffset].position = float4(float2(CULLED_VERTEX), 0.0, triangleBeginOffset);
    PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = float4(float2(CULLED_VERTEX), 0.0, 0.0);
    PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = float4(float2(CULLED_VERTEX), 0.0, 0.0);

    if (clipped.vertexCount < 3)
    {
        // printf("VERTEX COUNT = %i\n", clipped.vertexCount);
        return;
    }

    uint orig;
    InterlockedAdd(PushConst.metaDataBuffer[0].clipperEmittedVerticesCount, 3 * (clipped.vertexCount - 2), orig);

    // printf("[START]\n");

    for (uint i = 0; i < clipped.vertexCount - 2; i++)
    {
        // printf("\n[Thread-%u] Pre-processed\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n", globalThreadId.x,
        //         clipped.vertices[0].x, clipped.vertices[0].y, clipped.vertices[0].z, clipped.vertices[0].w,
        //         clipped.vertices[i+1].x, clipped.vertices[i+1].y, clipped.vertices[i+1].z, clipped.vertices[i+1].w,
        //         clipped.vertices[i+2].x, clipped.vertices[i+2].y, clipped.vertices[i+2].z, clipped.vertices[i+2].w);

        Triangle processed = ProcessPrimitive(clipped.vertices[0], clipped.vertices[i + 1], clipped.vertices[i + 2]);

        uint offset = uLongLifeGlobals.clipperEmittedVerticesOffset + orig + (i * 3);
        PushConst.intrinsicDataBuffer[offset].position = float4(processed.v0, 0.0, triangleBeginOffset + i);
        PushConst.intrinsicDataBuffer[offset + 1].position = float4(processed.v1, 0.0, 0.0);
        PushConst.intrinsicDataBuffer[offset + 2].position = float4(processed.v2, 0.0, 0.0);

        // printf("\n[Thread-%u] Processed\n%i, %i\n%i, %i\n%i, %i\n", globalThreadId.x,
        //         processed.v0.x, processed.v0.y,
        //         processed.v1.x, processed.v1.y,
        //         processed.v2.x, processed.v1.y);
    }

    // printf("[END]\n");
}

Triangle ProcessPrimitive(float4 vtxPosClipSpace_0, float4 vtxPosClipSpace_1, float4 vtxPosClipSpace_2)
{
    // Homogeneous divide (normalising projected view volume to canonical view / normalised device coordinates)
    // Canonical view volume / NDC: -1 <= X <= 1, -1 <= Y <= 1, 0 <= Z <= 1
    float4 vtxPosNdcSpace_0 = vtxPosClipSpace_0 / vtxPosClipSpace_0.w;
    float4 vtxPosNdcSpace_1 = vtxPosClipSpace_1 / vtxPosClipSpace_1.w;
    float4 vtxPosNdcSpace_2 = vtxPosClipSpace_2 / vtxPosClipSpace_2.w;

    // printf("\nPost divide\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n",
    //         vtxPosNdcSpace_0.x, vtxPosNdcSpace_0.y, vtxPosNdcSpace_0.z, vtxPosNdcSpace_0.w,
    //         vtxPosNdcSpace_1.x, vtxPosNdcSpace_1.y, vtxPosNdcSpace_1.z, vtxPosNdcSpace_1.w,
    //         vtxPosNdcSpace_2.x, vtxPosNdcSpace_2.y, vtxPosNdcSpace_2.z, vtxPosNdcSpace_2.w);

    int2 halfFramebuffer = int2(uLongLifeGlobals.framebufferSize * float2(0.5));

    // Debug:
    // PushConst.intrinsicDataBuffer[triangleBeginOffset].position = vtxPosNdcSpace_0;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 1].position = vtxPosNdcSpace_1;
    // PushConst.intrinsicDataBuffer[triangleBeginOffset + 2].position = vtxPosNdcSpace_2;

    int2 vtxPosRasterSpace_0 = int2(vtxPosNdcSpace_0.xy * halfFramebuffer);
    int2 vtxPosRasterSpace_1 = int2(vtxPosNdcSpace_1.xy * halfFramebuffer);
    int2 vtxPosRasterSpace_2 = int2(vtxPosNdcSpace_2.xy * halfFramebuffer);

    Triangle triangle = Triangle(vtxPosRasterSpace_0, vtxPosRasterSpace_1, vtxPosRasterSpace_2);

    // Cull triangles with no area
    int area = triangle.SignedArea();
    if (area == 0)
    {
        return Triangle(
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX),
        );
    }

    // Backface culling after homogenous divide is significantly simpler
#if BACKFACE_CULLING
    if (area > 0)
    {
        return Triangle(
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX),
            int2(CULLED_VERTEX),
        );
    }
#endif

    return triangle;
}

struct Triangle3D
{
    float4 a, b, c;
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
    float4 vertices[7];
    int vertexCount;

    [mutating]
    void AddVertex(float4 v)
    {
        // if (vertexCount >= 7) printf("Polygon vertex count out of bounds!");
        vertices[vertexCount] = v;
        vertexCount++;
    }

    static Polygon MakeNew()
    {
        Polygon p = Polygon();
        p.vertexCount = 0;
        p.vertices[0] = float4(0);
        p.vertices[1] = float4(0);
        p.vertices[2] = float4(0);
        p.vertices[3] = float4(0);
        p.vertices[4] = float4(0);
        p.vertices[5] = float4(0);
        p.vertices[6] = float4(0);
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

    // printf("\nOriginal\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n",
    //         triangle.a.x, triangle.a.y, triangle.a.z, triangle.a.w,
    //         triangle.b.x, triangle.b.y, triangle.b.z, triangle.b.w,
    //         triangle.c.x, triangle.c.y, triangle.c.z, triangle.c.w);

    const uint PLANE_COUNT = 5;
    float4 planeNormals[PLANE_COUNT] = {
        float4(0.0, 0.0, 1.0, 0.0),  // Near
        float4(-1.0, 0.0, 0.0, 1.0), // Left
        float4(1.0, 0.0, 0.0, 1.0),  // Right
        float4(0.0, 1.0, 0.0, 1.0),  // Top
        float4(0.0, -1.0, 0.0, 1.0), // Bottom
    };

    for (uint d = 0; d < PLANE_COUNT; d++)
    {
        Polygon clipped = Polygon::MakeNew();
        for (uint i = 0; i < p.vertexCount; i++)
        {
            int next = (i + 1) % p.vertexCount;
            D1 = dot(planeNormals[d], p.vertices[i]);
            D2 = dot(planeNormals[d], p.vertices[next]);

            if ((D1 >= 0) && (D2 >= 0))
            {
                clipped.AddVertex(p.vertices[i]);
            }
            else if ((D1 <= 0) && (D2 > 0))
            {
                float a = D1 / (D1 - D2);
                float4 intersection = float4(1.0 - a) * p.vertices[i] + a * p.vertices[next];
                clipped.AddVertex(intersection);
            }
            else if ((D1 > 0) && (D2 <= 0))
            {
                float a = D1 / (D1 - D2);
                float4 intersection = float4(1.0 - a) * p.vertices[i] + a * p.vertices[next];
                clipped.AddVertex(p.vertices[i]);
                clipped.AddVertex(intersection);
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
