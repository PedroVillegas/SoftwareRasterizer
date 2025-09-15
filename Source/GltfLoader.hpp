#pragma once

#include <optional>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include <Scene.hpp>

std::optional<Scene> LoadGltf(std::string_view filepath);

void LoadMaterials(Scene& scene, const fastgltf::Asset& gltf, const std::vector<uint32_t>& textures);

void LoadMeshes(Scene& scene, const fastgltf::Asset& gltf);

void LoadNodes(Scene& scene, const fastgltf::Asset& gltf);
