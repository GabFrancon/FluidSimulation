#pragma once

#include "vk_context.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;

    glm::vec3 camPos;
    glm::vec3 camFront;
    glm::vec3 camRight;
    glm::vec3 camUp;

    glm::vec3 worldFront;
    glm::vec3 worldUp;

    float pitch;
    float yaw;

    float fov;
    float near;
    float far;

    float mouseSensitivity = 0.05f;
    float keySensitivity   = 10.0f;

    Camera(){}

    Camera(
        glm::vec3 _camPos, 
        glm::vec3 _worldUp, 
        float _pitch = 0.0f, 
        float _yaw   = -90.0f, 
        float _fov   = glm::radians(45.0f), 
        float _near  = 0.1f,
        float _far   = 100.1f)
    {
        camPos  = _camPos;
        worldUp = _worldUp;
        pitch   = _pitch;
        yaw     = _yaw;
        fov     = _fov;
        near    = _near;
        far     = _far;
        updateCamVectors();
    }

    void processKeyboardInput(GLFWwindow* window, float dt) {
        float speed = keySensitivity * dt;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camPos += worldFront * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camPos -= worldFront * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camPos += camRight * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camPos -= camRight * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camPos += worldUp * speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camPos -= worldUp * speed;
    }

    void processMouseMovement(float xoffset, float yoffset)
    {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        updateCamVectors();
    }

    void updateCamVectors()
    {
        float x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        float y = std::sin(glm::radians(pitch));
        float z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));

        camFront = glm::normalize(glm::vec3(x, y, z));
        worldFront = glm::normalize(glm::vec3(x, 0.0, z));
        camRight = glm::normalize(glm::cross(camFront, worldUp));
        camUp = glm::normalize(glm::cross(camRight, camFront));
    }

    void updateViewMatrix() {
        viewMatrix = glm::lookAt(camPos, camPos + camFront, camUp);
    }

    void setOrthoProjection(float left, float right, float bottom, float top) {
        projMatrix = Camera::ortho(left, right, bottom, left, near, far);
    }

    void setPerspectiveProjection(float aspect) {
        projMatrix = Camera::perspective(fov, aspect, near, far);
    }

    static glm::mat4 perspective(float fov, float aspect, float near, float far) {
        float f = 1.0f / tan(0.5f * fov);

        glm::mat4 perspectiveProjectionMatrix = {
            f / aspect, 0.0f, 0.0f                       ,  0.0f,

            0.0f      , -f  , 0.0f                       ,  0.0f,

            0.0f      , 0.0f, far / (near - far)         , -1.0f,

            0.0f      , 0.0f, (near * far) / (near - far),  0.0f
        };

        return perspectiveProjectionMatrix;
    }

    static glm::mat4 ortho(float left, float right, float bottom, float top, float near, float far) {
        glm::mat4 orthographicProjectionMatrix = {
            2.0f / (right - left)           , 0.0f                            , 0.0f               , 0.0f,

            0.0f                            , 2.0f / (bottom - top)           , 0.0f               , 0.0f,

            0.0f                            , 0.0f                            , 1.0f / (near - far), 0.0f,

            -(right + left) / (right - left), -(bottom + top) / (bottom - top), near / (near - far), 1.0f
        };

        return orthographicProjectionMatrix;
    }

};

