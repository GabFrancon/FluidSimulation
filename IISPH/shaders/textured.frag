#version 460

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 proj;
    vec3 position;
} cameraData;

layout(set = 0, binding = 1) uniform SceneData {
    vec3 lightPosition;
    vec3 lightColor;
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
	vec3 N = normalize(fragNormal);
	vec3 L = normalize(sceneData.lightPosition - fragPosition);
	vec3 V = normalize(cameraData.position - fragPosition);
	vec3 H = normalize(L + V);

	float ambient  = 0.3f;
	float diffuse  = 1.0f * max( 0.0f, dot(N, L) );
	float specular = 0.3f * pow( max( 0.0f, dot(N, H) ), 32);

	vec4 light   = vec4( (ambient + diffuse + specular) * sceneData.lightColor, 1.0);
	vec4 texture = texture(texSampler, fragTexCoord);

    outColor = light * texture;
}