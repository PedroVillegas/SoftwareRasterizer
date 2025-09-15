#include "Camera.hpp"

#include <GLFW/glfw3.h>

glm::mat4 Camera::Update(GLFWwindow* pWindow, float dt, float speed, float sens)
{
    if (glfwGetKey(pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetInputMode(pWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        locked = true;
    }

    if (glfwGetKey(pWindow, GLFW_KEY_F) == GLFW_PRESS)
    {
        glfwSetInputMode(pWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        locked = false;
        firstMouse = true;
    }

    if (locked)
    {
        glm::quat pitchRotation = glm::angleAxis(glm::radians(rotation.x), glm::vec3 { 1.0F, 0.0F, 0.0F });
        glm::quat yawRotation = glm::angleAxis(glm::radians(rotation.y), glm::vec3 { 0.0F, 1.0F, 0.0F });

        glm::mat4 R = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
        glm::mat4 T = glm::translate(glm::mat4(1.0F), position);

        return glm::inverse(T * R);
    }

    double mousex = 0.0;
    double mousey = 0.0;
    glfwGetCursorPos(pWindow, &mousex, &mousey);
    glm::vec2 currentMousePosition = { mousex, mousey };

    if (firstMouse)
    {
        lastMousePosition = currentMousePosition;
        firstMouse = false;
    }

    glm::vec2 delta = currentMousePosition - lastMousePosition;
    lastMousePosition = currentMousePosition;

    if (delta.x != 0.0f || delta.y != 0.0f)
    {
        rotation.x -= delta.y * 0.05f; // pitch
        rotation.y -= delta.x * 0.05f; // yaw
    }

    rotation.y = glm::mod(rotation.y, 360.0F);
    rotation.x = glm::clamp(rotation.x, -89.0F, 89.0F);

    glm::quat pitchRotation = glm::angleAxis(glm::radians(rotation.x), glm::vec3 { 1.0F, 0.0F, 0.0F });
    glm::quat yawRotation = glm::angleAxis(glm::radians(rotation.y), glm::vec3 { 0.0F, 1.0F, 0.0F });

    glm::mat4 R = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);

    glm::vec3 move = glm::vec3(0.0F);
    if (glfwGetKey(pWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        move.z -= 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        move.z += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        move.x -= 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        move.x += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        move.y += 1.0F;
    }
    if (glfwGetKey(pWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        move.y -= 1.0F;
    }
    if (glm::any(glm::notEqual(move, glm::vec3(0.0F))))
    {
        position += glm::normalize(glm::mat3(R) * move) * speed * dt;
    }
    glm::mat4 T = glm::translate(glm::mat4(1.0F), position);

    return glm::inverse(T * R);
}
