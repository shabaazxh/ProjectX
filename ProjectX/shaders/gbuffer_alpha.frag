#version 450

layout(location = 0) in vec4 WorldPos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 WorldNormal;

layout(location = 0) out vec4 albedo;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 roughness;
layout(location = 3) out vec4 metallic;
layout(location = 4) out vec4 emissive;

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

layout(set = 0, binding = 1) uniform texture2D textures[200];
layout(set = 0, binding = 2) uniform sampler samplerAnisotropic;


void main()
{
	vec4 color = texture(sampler2D(textures[pc.dTextureID], samplerAnisotropic), uv);
	if(color.a < 0.1)
	{
		discard;
	}
	albedo = color;
}
