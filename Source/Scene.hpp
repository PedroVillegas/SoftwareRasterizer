#pragma once

struct Vertex
{
    glm::vec3 position = { 0.0F, 0.0F, 0.0F };
    glm::vec2 uv = { 0.0F, 0.0F };
};

struct Material
{
    glm::vec4 albedo;
    glm::vec4 emissionFactorStrength;
    float roughness;
    float metallic;
    float specular;
    float ior;
    float clearcoat;
    uint32_t albedoMapId;
    uint32_t metalRoughMapId;
    uint32_t normalMapId;
    uint32_t occlusionMapId;
    uint32_t emissionMapId;
};

struct Mesh
{
    uint32_t firstVertex;
    uint32_t firstIndex;
    uint32_t indicesCount;
    uint32_t transformId;
    uint32_t materialId;
};

struct Scene
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
};
