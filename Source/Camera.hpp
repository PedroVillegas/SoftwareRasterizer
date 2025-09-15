#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

struct GLFWwindow;

struct Camera
{
    glm::vec3 position = { 0.0F, 0.0F, 0.0F };
    glm::vec3 rotation = { 0.0F, 0.0F, 0.0F };
    glm::vec2 lastMousePosition = { 0.0F, 0.0F };
    bool locked = true;
    bool firstMouse = true;

    glm::mat4 Update(GLFWwindow* pWindow, float dt, float speed, float sens);
};
