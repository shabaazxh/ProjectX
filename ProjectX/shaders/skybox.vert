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
}pc;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 tex;

layout(location = 0) out vec3 texCoords;

void main()
{
	texCoords = pos;
	mat4 rotView = mat4(mat3(ubo.view));
	vec4 upos = ubo.projection * rotView * pc.ModelMatrix * vec4(pos, 1.0);
	gl_Position = upos.xyww;
}
