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


layout(set = 0, binding = 1) uniform Light
{
    vec4 LightPosition;
    vec4 LightColour;
    mat4 LightSpaceMatrix;
}lightubo;

layout(push_constant) uniform Push
{
	mat4 ModelMatrix;
	uint dTextureID; // diffuse 
	uint mTextureID; // metalness
	uint rTextureID; // roughness
}pc;

layout(set = 0, binding = 2) uniform texture2D textures[200];
layout(set = 0, binding = 3) uniform sampler samplerAnisotropic;
layout(set = 0, binding = 4) uniform sampler samplerNormal;

// define an array of colors for each mip level
const vec4 colors[10] = { 
    vec4(1.0, 0.0, 0.0, 1.0), 
    vec4(0.0, 1.0, 0.0, 1.0), 
    vec4(0.0, 0.0, 1.0, 1.0), 
    vec4(1.0, 1.0, 0.0, 1.0), 
    vec4(0.0, 1.0, 1.0, 1.0), 
    vec4(1.0, 0.0, 1.0, 1.0), 
    vec4(1.0, 0.5, 0.0, 1.0), 
    vec4(0.5, 0.0, 1.0, 1.0), 
    vec4(0.5, 0.5, 0.5, 1.0), 
    vec4(1.0, 1.0, 1.0, 1.0)
};

void main()
{
    // get the LOD which would be used to sample from the texture
    // .y = "level of detail relative to base level is returned in y" - https://registry.khronos.org/OpenGL-Refpages/gl4/html/textureQueryLod.xhtml
    vec2 loadInfo = textureQueryLod(sampler2D(textures[pc.dTextureID], samplerNormal), uv);
    int numMips = textureQueryLevels(sampler2D(textures[pc.dTextureID], samplerNormal));

    float lod = loadInfo.y; // mip level in use
    int index = int(floor(lod)); // get integer
    float blendFactor = fract(lod); // fractional part to blend later

    int clampedIndex = clamp(index + 1, 0, numMips - 1); // clamp next mip level index
    vec4 baseColor = colors[index]; // get current mip color
    vec4 nextColor = colors[clampedIndex]; // get next mip color

    vec4 blendColor = mix(baseColor, nextColor, blendFactor); // blend using mix 
    fragColor = vec4(blendColor.xyz, 1.0); 
}
