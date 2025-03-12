#version 450

layout(location = 0) in vec4 WorldPos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 WorldNormal;

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

void main()
{
	float dz_dx = dFdx(gl_FragCoord.z);
	float dz_dy = dFdy(gl_FragCoord.z);
	fragColor = vec4(vec3(abs(dz_dx), abs(dz_dy), 0.0) * 1000.0, 1.0);
}
