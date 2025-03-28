#version 450

layout(location = 0) in vec3 texCoords;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform SceneUniform
{
	mat4 model;
	mat4 view;
	mat4 projection;
    vec4 cameraPosition;
    vec2 viewportSize;
	float fov;
	float nearPlane;
	float farPlane;
} ubo;

layout(set = 0, binding = 1) uniform samplerCube cubemap;

void main()
{
	fragColor = texture(cubemap, texCoords);
}
