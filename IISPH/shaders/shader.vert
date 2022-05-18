#version 450

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
} cameraData;

layout(set = 0, binding = 1) uniform ObjectData {
    mat4 model;
} objectData;


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {

    gl_Position = cameraData.proj * cameraData.view * objectData.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}