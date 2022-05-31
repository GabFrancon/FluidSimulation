#version 460

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
    vec3 position;
} cameraData;

struct ObjectData{
	mat4 model;
    vec3 albedo;
};

layout(std140, set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectsBuffer;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragColor;

void main() {
    ObjectData objectData = objectsBuffer.objects[gl_InstanceIndex];

    gl_Position = cameraData.proj * cameraData.view * objectData.model * vec4(inPosition, 1.0);

    fragPosition = inPosition;
    fragNormal   = inNormal;
    fragTexCoord = inTexCoord;
    fragColor    = objectData.albedo;
}