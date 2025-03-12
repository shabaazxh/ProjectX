#version 450

layout(location = 0) in vec4 WorldPos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 WorldNormal;
layout(location = 3) in mat3 TBN;

layout(location = 0) out vec4 albedo;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 emissive;
layout(location = 3) out vec2 metroughness;

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
	uint nTextureID; // normalMap
}pc;

layout(set = 0, binding = 1) uniform texture2D textures[200];
layout(set = 0, binding = 2) uniform sampler samplerAnisotropic;


void main()
{
	vec4 color = texture(sampler2D(textures[pc.dTextureID], samplerAnisotropic), uv);
	albedo = color;
	//normal = vec4(WorldNormal) * 0.5 + 0.5;	
	vec3 texNormal = texture(sampler2D(textures[pc.nTextureID], samplerAnisotropic), uv).rgb * 2.0 - 1.0;

	texNormal = (TBN * texNormal);
	texNormal = normalize(texNormal);
	normal = vec4(texNormal * 0.5 + 0.5, 0.0); // convert back to 0-1 for storage because of normal texture format

	metroughness.r = texture(sampler2D(textures[pc.rTextureID], samplerAnisotropic), uv).r;
	metroughness.g = texture(sampler2D(textures[pc.mTextureID], samplerAnisotropic), uv).r;

	emissive = pc.eTextureID == -1 ? vec4(0.0, 0.0, 0.0, 1.0) : texture(sampler2D(textures[pc.eTextureID], samplerAnisotropic), uv) * 100.0;
}
