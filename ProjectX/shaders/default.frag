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

struct Light
{
	int Type;
	vec4 LightPosition;
	vec4 LightColour;
	mat4 LightSpaceMatrix;
};

const int NUM_LIGHTS = 26;

layout(set = 0, binding = 1) uniform LightBuffer {
	Light lights[NUM_LIGHTS];
} lightData;

layout(push_constant) uniform Push
{
	mat4 ModelMatrix;
	uint dTextureID; // diffuse 
	uint mTextureID; // metalness
	uint rTextureID; // roughness
	uint eTextureID; // emissive
}pc;

layout(set = 0, binding = 2) uniform texture2D textures[200];
layout(set = 0, binding = 3) uniform sampler samplerAnisotropic;
layout(set = 0, binding = 4) uniform sampler samplerNormal;
layout(set = 0, binding = 5) uniform sampler2DShadow shadowMap;

#define PI 3.14159265359

// Fresnel (shlick approx)
vec3 Fresnel(vec3 halfVector, vec3 viewDir, vec3 baseColor, float metallic)
{
    vec3 F0 = vec3(0.04);
    F0 = (1 - metallic) * F0 + (metallic * baseColor);
    float HdotV = max(dot(halfVector, viewDir), 0.0);
    vec3 schlick_approx = F0 + (1 - F0) * pow(clamp(1 - HdotV, 0.0, 1.0), 5);
    return schlick_approx;
}

// Normal distribution function
float BeckmannNormalDistribution(vec3 normal, vec3 halfVector, float roughness)
{
    float a = roughness * roughness;
	float a2 = a * a; // alpha is roughness squared
	float NdotH = max(dot(normal, halfVector), 0.001); // preventing divide by zero
	float NdotHSquared = NdotH * NdotH;
	float numerator = exp((NdotHSquared - 1.0) / (a2 * NdotHSquared));
	float denominator = PI * a2 * (NdotHSquared * NdotHSquared); // pi * a2 * (n * h)^4

	float D = numerator / denominator;
	return D;
}

// Geometry term
float GeometryTerm(vec3 normal, vec3 halfVector, vec3 lightDir, vec3 viewDir)
{
	float NdotH = max(dot(normal, halfVector), 0.0);
	float NdotV = max(dot(normal, viewDir), 0.0);
	float VdotH = max(dot(viewDir, halfVector), 0.0);
	float NdotL = max(dot(normal, lightDir), 0.0);

	float term1 = 2 * (NdotH * NdotV) / VdotH;
	float term2 = 2 * (NdotH * NdotL) / VdotH;

	float G = min(1, min(term1, term2));

	return G;
}

// Compute BRDF 
vec3 CookTorranceBRDF(vec3 normal, vec3 halfVector, vec3 viewDir, vec3 lightDir, float metallic, float roughness, vec3 baseColor, vec3 LightColour)
{
    vec3 F = Fresnel(halfVector, viewDir, baseColor, metallic);
    float D = BeckmannNormalDistribution(normal, halfVector, roughness);
	float G = GeometryTerm(normal, halfVector, lightDir, viewDir);

    vec3 ambient = vec3(0.02);
    vec3 L_Diffuse = (baseColor.xyz / PI) * (vec3(1,1,1) - F) * (1.0 - metallic);

    float NdotV = max(dot(normal, viewDir), 0.0);
	float NdotL = max(dot(normal, lightDir), 0.0);

	vec3 numerator = D * G * F;
	float denominator = (4 * NdotV * NdotL) + 0.001;

	vec3 specular = numerator / denominator;

    vec3 outLight = (L_Diffuse + specular) * LightColour.xyz * NdotL;

    return vec3(outLight);
}

void main()
{
	vec4 color = texture(sampler2D(textures[pc.dTextureID], samplerAnisotropic), uv);
	vec3 emissive = vec3(0.0f);
	if(pc.eTextureID != -1)
	{
		emissive = texture(sampler2D(textures[pc.eTextureID], samplerAnisotropic), uv).rgb;
	}

    vec3 wNormal = normalize(WorldNormal).xyz;

    // == Metal and Roughness ==
    float roughness = texture(sampler2D(textures[pc.rTextureID], samplerAnisotropic), uv).x;
    float metallic = texture(sampler2D(textures[pc.mTextureID], samplerAnisotropic), uv).x;

    vec3 outLight = vec3(0.0);

	for(int i = 0; i < NUM_LIGHTS; i++)
	{
		vec3 lightDir = normalize(lightData.lights[i].LightPosition.xyz - WorldPos.xyz);
		vec3 viewDir = normalize(ubo.cameraPosition.xyz - WorldPos.xyz);
		vec3 halfVector = normalize(viewDir + lightDir);

		// is it a spot light?
		vec3 LightColour = vec3(0.0);
		bool isDirectional = lightData.lights[i].Type == 1 ? false : true;

		if(!isDirectional) 
		{
			float dist = length(lightData.lights[i].LightPosition.xyz - WorldPos.xyz);
			float att = 1.0 / (dist * dist); 
			LightColour = lightData.lights[i].LightColour.xyz * att;
		}
		else {
			LightColour = lightData.lights[i].LightColour.rgb;
		}

		if(isDirectional) {
			//float shadowTerm = 1.0 - PCF(WorldPos, wNormal);
			outLight += CookTorranceBRDF(wNormal, halfVector, viewDir, lightDir, metallic, roughness, color.xyz, LightColour);
			
		} 
		else {
			outLight += CookTorranceBRDF(wNormal, halfVector, viewDir, lightDir, metallic, roughness, color.xyz, LightColour);
		}
	}

	vec3 ambient = vec3(0.02) * color.xyz;
	outLight += ambient;
	fragColor = vec4(vec3(outLight + emissive), 1.0);
	
}
