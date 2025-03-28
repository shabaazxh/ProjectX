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

// Linearize the depth value of the depth buffer
float LinearizeDepth()
{
    float d = gl_FragCoord.z;
    float zNear = ubo.nearPlane;
    float zFar = ubo.farPlane;
	return (zFar * zNear) / (zFar - zNear) / (d + zFar / (zNear - zFar));
}

void main()
{
	float linDepth = LinearizeDepth();
	// normalize
	float normDepth = -(linDepth - ubo.nearPlane) / (ubo.farPlane - ubo.nearPlane); // negate since lin depth is negative
	fragColor = vec4(normDepth, normDepth, normDepth, 1.0);
}
