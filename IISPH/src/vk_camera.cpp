#include "vk_camera.h"

void Camera::processKeyboardInput(GLFWwindow* window, float dt) {
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

    updateViewMatrix();
}

void Camera::processMouseMovement(float xoffset, float yoffset)
{
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    updateCamVectors();
}

void Camera::updateCamVectors()
{
    float x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
    float y = std::sin(glm::radians(pitch));
    float z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));

    camFront   = glm::normalize(glm::vec3(x, y, z));
    worldFront = glm::normalize(glm::vec3(x, 0.0f, z));
    camRight   = glm::normalize(glm::cross(camFront, worldUp));
    camUp      = glm::normalize(glm::cross(camRight, camFront));
}

void Camera::updateViewMatrix() {
    viewMatrix = glm::lookAt(camPos, camPos + camFront, camUp);
}

void Camera::setOrthoProjection(float left, float right, float bottom, float top) {
    projMatrix = Camera::ortho(left, right, bottom, left, near, far);
}

void Camera::setPerspectiveProjection(float aspect) {
    projMatrix = Camera::perspective(fov, aspect, near, far);
}

void Camera::panoramaView(glm::vec3 center, float radius, float altitude, float timePeriod, float dt) {
    static float time = 0.0f;
    time += dt;
    glm::vec3 newPos{};
    newPos.x = glm::sin(2 * M_PI * time / timePeriod) * radius;
    newPos.y = altitude;
    newPos.z = glm::cos(2 * M_PI * time / timePeriod) * radius;

    viewMatrix = glm::lookAt(newPos + center, center, worldUp);
}

glm::mat4 Camera::perspective(float fov, float aspect, float near, float far) {
    float f = 1.0f / tan(0.5f * fov);

    glm::mat4 perspectiveProjectionMatrix = {
        f / aspect, 0.0f, 0.0f                       ,  0.0f,

        0.0f      , -f  , 0.0f                       ,  0.0f,

        0.0f      , 0.0f, far / (near - far)         , -1.0f,

        0.0f      , 0.0f, (near * far) / (near - far),  0.0f
    };

    return perspectiveProjectionMatrix;
}

glm::mat4 Camera::ortho(float left, float right, float bottom, float top, float near, float far) {
    glm::mat4 orthographicProjectionMatrix = {
        2.0f / (right - left)           , 0.0f                            , 0.0f               , 0.0f,

        0.0f                            , 2.0f / (bottom - top)           , 0.0f               , 0.0f,

        0.0f                            , 0.0f                            , 1.0f / (near - far), 0.0f,

        -(right + left) / (right - left), -(bottom + top) / (bottom - top), near / (near - far), 1.0f
    };

    return orthographicProjectionMatrix;
}