#include "GltfLoader.hpp"

#include <spdlog/spdlog.h>

std::optional<Scene> LoadGltf(std::string_view filepath)
{
    Scene scene;

    auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember;
    gltfOptions |= fastgltf::Options::AllowDouble;
    gltfOptions |= fastgltf::Options::LoadExternalBuffers;
    // gltfOptions |= fastgltf::Options::LoadExternalImages;

    fastgltf::Asset gltf;
    fastgltf::Parser parser {};

    const std::filesystem::path path = filepath;

    auto gltfFile = fastgltf::GltfDataBuffer::FromPath(filepath);

    if (gltfFile.error() != fastgltf::Error::None)
    {
        spdlog::error("File does not exist ({})", filepath);
        assert(std::filesystem::exists(filepath));
        return {};
    }
    spdlog::info("Loaded gltf file ({})", filepath);

    auto type = fastgltf::determineGltfFileType(gltfFile.get());
    if (type == fastgltf::GltfType::glTF)
    {
        auto load = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            spdlog::error("Failed to load gltf file ({})", fastgltf::to_underlying(load.error()));
            return {};
        }
    }
    else if (type == fastgltf::GltfType::GLB)
    {
        auto load = parser.loadGltfBinary(gltfFile.get(), path.parent_path(), gltfOptions);
        if (load)
        {
            gltf = std::move(load.get());
        }
        else
        {
            spdlog::error("Failed to load glTF: {}", fastgltf::to_underlying(load.error()));
            return {};
        }
    }
    else
    {
        spdlog::error("Failed to determine gltf container");
        return {};
    }

    // Load samplers
    // for (fastgltf::Sampler& sampler : gltf.samplers)
    // {
    //     SamplerDesc desc = {
    //         .minFilter = ExtractFilter(sampler.minFilter.value_or(fastgltf::Filter::Linear)),
    //         .magFilter = ExtractFilter(sampler.magFilter.value_or(fastgltf::Filter::Linear)),
    //         .addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    //         .mipmapMode = ExtractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Linear))
    //     };
    //
    //     file.samplers.emplace_back(device, desc);
    // }

    // Temporary gltf data storage
    std::vector<uint32_t> textures;

    // Begin loading file
    // MeshNodes depend on meshes, meshes depend on materials, and materials on textures
    // so begin with textures

    // #if LOAD_TEXTURES
    //     LoadTextures(file, gltf, path, textures);
    // #endif

    LoadMaterials(scene, gltf, textures);

    LoadMeshes(scene, gltf);

    LoadNodes(scene, gltf);

    return std::move(scene);
}

void LoadMaterials(Scene& scene, const fastgltf::Asset& gltf, const std::vector<uint32_t>& textures)
{
    auto GetIfHasTexture = [&](auto& texture, auto& mapLoc)
    {
        if (texture.has_value())
        {
            if (gltf.textures[texture.value().textureIndex].imageIndex.has_value())
            {
                size_t img = gltf.textures[texture.value().textureIndex].imageIndex.value();
                mapLoc = textures[img];
            }
        }
    };

    for (const fastgltf::Material& mat : gltf.materials)
    {
        scene.materials.emplace_back();
        Material& newMat = scene.materials.back();

        newMat.albedo.x = mat.pbrData.baseColorFactor[0];
        newMat.albedo.y = mat.pbrData.baseColorFactor[1];
        newMat.albedo.z = mat.pbrData.baseColorFactor[2];

        newMat.emissionFactorStrength.x = mat.emissiveFactor[0];
        newMat.emissionFactorStrength.y = mat.emissiveFactor[1];
        newMat.emissionFactorStrength.z = mat.emissiveFactor[2];
        newMat.emissionFactorStrength.w = mat.emissiveStrength;

        newMat.metallic = mat.pbrData.metallicFactor;
        newMat.roughness = mat.pbrData.roughnessFactor;
        newMat.ior = mat.ior;

        if (mat.specular)
            newMat.specular = mat.specular->specularFactor;

        if (mat.clearcoat)
            newMat.clearcoat = mat.clearcoat->clearcoatFactor;

        // Grab textures from gltf file
        // GetIfHasTexture(mat.pbrData.baseColorTexture, newMat.albedoMapId);
        // GetIfHasTexture(mat.pbrData.metallicRoughnessTexture, newMat.metalRoughMapId);
        // GetIfHasTexture(mat.normalTexture, newMat.normalMapId);
        // GetIfHasTexture(mat.occlusionTexture, newMat.occlusionMapId);
        // GetIfHasTexture(mat.emissiveTexture, newMat.emissionMapId);
    }
}

void LoadMeshes(Scene& scene, const fastgltf::Asset& gltf)
{
    // Use the same vectors for all meshes so that the memory doesn't reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> meshIds;
    for (const fastgltf::Mesh& mesh : gltf.meshes)
    {
        // Clear the mesh arrays each mesh, we don't want to merge them by error
        indices.clear();
        vertices.clear();
        meshIds.clear();
        meshIds.resize(mesh.primitives.size());

        uint32_t primitiveId = 0;
        for (auto&& p : mesh.primitives)
        {
            scene.meshes.emplace_back();
            Mesh& newMesh = scene.meshes.back();

            size_t initial_vtx = vertices.size();

            // Load indices
            if (p.indicesAccessor.has_value())
            {
                const fastgltf::Accessor& accessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + accessor.count);
                meshIds[primitiveId] = accessor.count / 3;

                fastgltf::iterateAccessor<std::uint32_t>(gltf,
                                                         accessor,
                                                         [&](std::uint32_t idx)
                                                         {
                                                             indices.push_back(idx + (uint32_t) initial_vtx);
                                                         });
            }

            // Load vertex positions
            if (p.indicesAccessor.has_value())
            {
                const fastgltf::Accessor& accessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + accessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf,
                                                              accessor,
                                                              [&](glm::vec3 v, size_t index)
                                                              {
                                                                  Vertex newvtx;
                                                                  newvtx.position = v;
                                                                  // newvtx.normal = { 1, 0, 0 };
                                                                  // newvtx.colour = glm::vec4 { 1.f };
                                                                  newvtx.uv = { 0, 0 };
                                                                  vertices[initial_vtx + index] = newvtx;
                                                              });
            }

            // Load vertex normals
            // auto normals = p.findAttribute("NORMAL");
            // if (normals != p.attributes.end())
            // {
            //     fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf,
            //                                                   gltf.accessors[(*normals).accessorIndex],
            //                                                   [&](glm::vec3 v, size_t index)
            //                                                   {
            //                                                       vertices[initial_vtx + index].normal = v;
            //                                                   });
            // }

            // Load vertex tangents
            // auto tangents = p.findAttribute("TANGENT");
            // if (tangents != p.attributes.end())
            // {
            //     fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf,
            //                                                   gltf.accessors[(*tangents).accessorIndex],
            //                                                   [&](glm::vec4 v, size_t index)
            //                                                   {
            //                                                       vertices[initial_vtx + index].tangent = v;
            //                                                   });
            // }

            // Load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf,
                                                              gltf.accessors[(*uv).accessorIndex],
                                                              [&](glm::vec2 v, size_t index)
                                                              {
                                                                  vertices[initial_vtx + index].uv = v;
                                                              });
            }

            // Load vertex colors
            // auto colors = p.findAttribute("COLOR_0");
            // if (colors != p.attributes.end())
            // {
            //     fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf,
            //                                                   gltf.accessors[(*colors).accessorIndex],
            //                                                   [&](glm::vec4 v, size_t index)
            //                                                   {
            //                                                       vertices[initial_vtx + index].colour = v;
            //                                                   });
            // }

            // Get the index to material in GLTFs list of materials
            newMesh.materialId = static_cast<uint32_t>(p.materialIndex.value_or(0));
            primitiveId++;

            scene.indices.insert(scene.indices.end(), indices.begin(), indices.end());
            scene.vertices.insert(scene.vertices.end(), vertices.begin(), vertices.end());

            newMesh.firstVertex = scene.vertices.size();
            newMesh.firstIndex = scene.indices.size();
            newMesh.indicesCount = indices.size();
        }
    }
}

void LoadNodes(Scene& scene, const fastgltf::Asset& gltf)
{
    /*
    // Load all nodes and their meshes
    for (const fastgltf::Node& node : gltf.nodes)
    {
        scene.nodes.emplace_back();
        Node& newNode = scene.nodes.back();
        newNode.name = node.name;

        if (node.meshIndex.has_value())
        {
            newNode.meshIndex = static_cast<uint32_t>(node.meshIndex.value());
        }

        std::visit(
            fastgltf::visitor {
                [&](fastgltf::TRS transform)
                {
                    glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                    glm::quat rot(
                        transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                    glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                    glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                    glm::mat4 rm = glm::toMat4(rot);
                    glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                    newNode.localTransform.translation = tl;
                    newNode.localTransform.rotation = glm::eulerAngles(rot);
                    newNode.localTransform.scale = sc;
                    newNode.localTransform.transform = tm * rm * sm;
                },
                [&](fastgltf::math::fmat4x4 matrix)
                {
                    newNode.localTransform.transform = glm::make_mat4(matrix.data());
                    newNode.localTransform.translation = newNode.localTransform.transform[3];

                    newNode.localTransform.scale = glm::vec3(glm::length(newNode.localTransform.transform[0]),
                                                             glm::length(newNode.localTransform.transform[1]),
                                                             glm::length(newNode.localTransform.transform[2]));

                    glm::mat4 rot =
                        glm::mat4(glm::vec4(newNode.localTransform.transform[0] / newNode.localTransform.scale.x),
                                  glm::vec4(newNode.localTransform.transform[1] / newNode.localTransform.scale.y),
                                  glm::vec4(newNode.localTransform.transform[2] / newNode.localTransform.scale.z),
                                  glm::vec4(newNode.localTransform.transform[3]));

                    newNode.localTransform.rotation = glm::eulerAngles(glm::normalize(glm::quat_cast(rot)));
                } },
            node.transform);
    }

    // Run loop again to set up transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++)
    {
        const fastgltf::Node& node = gltf.nodes[i];
        Node& sceneNode = scene.nodes[i];
        for (const auto& child : node.children)
        {
            sceneNode.children.push_back(static_cast<uint32_t>(child));
            sceneNode.parent = i;
        }
    }

    // Find the top nodes, with no parents
    for (uint32_t i = 0; i < scene.nodes.size(); ++i)
    {
        Node& node = scene.nodes[i];
        if (node.parent.has_value())
        {
            scene.topNodes.push_back(i);
            node.RefreshTransform(scene.nodes, {});
        }
    }
    */
}
