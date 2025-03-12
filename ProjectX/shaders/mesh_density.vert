#version 450


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

layout(push_constant) uniform Push
{
    mat4 ModelMatrix;
    uint dTextureID; // diffuse 
    uint mTextureID; // metalness
    uint rTextureID; // roughness
    uint eTextureID; // emissive
}pc;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 WorldNormal;
layout(location = 2) out vec4 WorldPosition;

void main()
{
	WorldNormal = normal;
	uv = tex;
	WorldPosition = pc.ModelMatrix * vec4(position, 1.0);
	gl_Position = ubo.view * pc.ModelMatrix * vec4(position, 1.0);
}