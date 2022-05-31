#pragma once

#ifndef M_PI
#define M_PI 3.141592
#endif

#include "vk_context.h"
#include "vk_mesh.h"

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

    float fov;
    float near;
    float far;

    float pitch = 0.0f;
    float yaw = -90.0f;

    float mouseSensitivity = 0.05f;
    float keySensitivity   = 10.0f;

    Camera(){}

    Camera(
        glm::vec3 _camPos, 
        glm::vec3 _worldUp, 
        float _fov   = glm::radians(45.0f), 
        float _near  = 0.1f,
        float _far   = 150.1f)
    {
        camPos  = _camPos;
        worldUp = _worldUp;
        fov     = _fov;
        near    = _near;
        far     = _far;
        updateCamVectors();
    }

    void processKeyboardInput(GLFWwindow* window, float dt);
    void processMouseMovement(float xoffset, float yoffset);
    void updateCamVectors();

    void updateViewMatrix();
    void setOrthoProjection(float left, float right, float bottom, float top);
    void setPerspectiveProjection(float aspect);

    void panoramaView(glm::vec3 center, float radius, float altitude, float timePeriod, float dt);

    static glm::mat4 perspective(float fov, float aspect, float near, float far);
    static glm::mat4 ortho(float left, float right, float bottom, float top, float near, float far);
};

