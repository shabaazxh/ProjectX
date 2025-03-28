#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColours;

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


layout(set = 0, binding = 2) uniform sampler2D depthTex;
layout(set = 0, binding = 3) uniform sampler2D gAlbedo;
layout(set = 0, binding = 4) uniform sampler2D gNormal;
layout(set = 0, binding = 5) uniform sampler2D gMetRoughness;
layout(set = 0, binding = 6) uniform sampler2D gEmissive;
layout(set = 0, binding = 7) uniform sampler2DShadow shadowMap;

#define PI 3.14159265359

// Reconstruct world position from depth
// UV and depth produce screen space position
// Multiply screen space x and y by 2.0 - 1.0 to get into clip space (NDC)
// unproject the NDC to get into view-space
// use inverse view matrix on the view-space position to get back into world position
vec3 DepthToWorldPos()
{
	float depth = texture(depthTex, uv).x;
	vec3 clipSpace = vec3(2.0 * uv - 1.0, depth);
	vec4 viewSpace = inverse(ubo.projection) * vec4(clipSpace.xyz, 1.0);
	viewSpace.xyz /= viewSpace.w;

	vec4 worldPos = inverse(ubo.view) * vec4(viewSpace.xyz, 1.0);
	return worldPos.xyz;
}

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

// Cook-Torrance Geometry term
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


// ======================================================================
// GGX
// https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// ======================================================================

float GGXNormalDistributionFunction(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.001);
	float NdotH2 = NdotH * NdotH;

	float numerator = a2;
	float denominator = ((NdotH2) * (a2 - 1.0) + 1.0);
	denominator = PI * denominator * denominator;

	return numerator / max(denominator, 0.001);
}

float GGXGeometrySchlick(float NdotV, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotV2 = NdotV * NdotV;

	float numerator   = 2.0 * NdotV;
	float denominator = NdotV + sqrt(a2 + (1.0 - a2) * NdotV2);

	return numerator / max(denominator, 0.001);
}

float GGXGeometrySmith(vec3 normal, vec3 lightDir, vec3 viewDir, float roughness)
{
	float NdotV = max(dot(normal, viewDir), 0.001);
	float NdotL = max(dot(normal, lightDir), 0.001);

	float ggx1 = GGXGeometrySchlick(NdotV, roughness);
	float ggx2 = GGXGeometrySchlick(NdotL, roughness);

	return ggx1 * ggx2;
}

// Compute BRDF
vec3 CookTorranceBRDF(vec3 normal, vec3 halfVector, vec3 viewDir, vec3 lightDir, float metallic, float roughness, vec3 baseColor, vec3 LightColour)
{
    vec3 F = Fresnel(halfVector, viewDir, baseColor, metallic);
    float D = GGXNormalDistributionFunction(normal, halfVector, roughness);
	float G = GGXGeometrySmith(normal, lightDir, viewDir, roughness);

    vec3 L_Diffuse = (baseColor / PI) * (vec3(1.0) - F) * (1.0 - metallic);

    float NdotV = max(dot(normal, viewDir), 0.001);
	float NdotL = max(dot(normal, lightDir), 0.001);

	vec3 numerator = D * G * F;
	float denominator = (4 * NdotV * NdotL) + 0.001;

	vec3 specular = numerator / denominator;

    vec3 outLight = (L_Diffuse + specular) * LightColour.xyz * NdotL;

    return vec3(outLight);
}

float Shadow(vec3 WorldPos)
{
	vec4 fragPositionInLightSpace = lightData.lights[0].LightSpaceMatrix * vec4(WorldPos, 1.0);
	fragPositionInLightSpace.xyz /= fragPositionInLightSpace.w;
	fragPositionInLightSpace.xy = fragPositionInLightSpace.xy * 0.5 + 0.5;
	fragPositionInLightSpace.z -= 0.005;

	float shadow = textureProj(shadowMap, fragPositionInLightSpace);
	return shadow;
}

// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
float PCF(vec3 WorldPos)
{
	// Use direct lighting only. Point light shadows are handleded differently (cube depth)
	vec4 fragPositionInLightSpace = lightData.lights[0].LightSpaceMatrix * vec4(WorldPos, 1.0);
	fragPositionInLightSpace.xyz /= fragPositionInLightSpace.w;
	fragPositionInLightSpace.xy = fragPositionInLightSpace.xy * 0.5 + 0.5;

	vec2 texSize = 1.0 / textureSize(shadowMap, 0);
	int range = 2; // 4x4
	int samples = 0;
	float sum = 0.0;
	for(int x = -range; x < range; x++)
	{
		for(int y = -range; y < range; y++)
		{
			vec2 offset = vec2(x,y) * texSize;
			vec4 sampleCoord = vec4(fragPositionInLightSpace.xy + offset, fragPositionInLightSpace.z - 0.005, fragPositionInLightSpace.w);
			sum += textureProj(shadowMap, sampleCoord);
			samples++;
		}
	}

	return sum / float(samples);
}


void main()
{
	vec4 color = texture(gAlbedo, uv);
	vec3 emissive = texture(gEmissive, uv).rgb;
	vec3 WorldPos = DepthToWorldPos();
    vec3 wNormal = normalize(texture(gNormal, uv).xyz * 2.0 - 1.0);

	vec3 outLight = vec3(0.0f);

	for(int i = 0; i < NUM_LIGHTS; i++)
	{
		vec3 lightDir = normalize(lightData.lights[i].LightPosition.xyz - WorldPos.xyz);
		vec3 viewDir = normalize(ubo.cameraPosition.xyz - WorldPos.xyz);
		vec3 halfVector = normalize(viewDir + lightDir);

		// == Metal and Roughness ==
		float roughness = max(texture(gMetRoughness, uv).r, 0.1);
		float metallic = texture(gMetRoughness, uv).g;

		// is it a spot light?
		vec3 LightColour = vec3(0.0);
		bool isDirectional = lightData.lights[i].Type == 1 ? false : true;

		// If not directional light, calculate attenuation
		if(!isDirectional)
		{
			float dist = length(lightData.lights[i].LightPosition.xyz - WorldPos);
			float att = 1.0 / (dist * dist);
			LightColour = lightData.lights[i].LightColour.xyz * att;
		}
		else {
			lightDir = normalize(-lightData.lights[i].LightPosition.xyz);
			LightColour = lightData.lights[i].LightColour.rgb;
			halfVector = normalize(viewDir + lightDir);
		}

		// Apply shadow only to direct lighting
		if(isDirectional) {
			float shadowCoefficent = 1.0 - PCF(WorldPos);
			outLight += shadowCoefficent * CookTorranceBRDF(wNormal, halfVector, viewDir, lightDir, metallic, roughness, color.xyz, LightColour);

		}
		else {
			outLight += CookTorranceBRDF(wNormal, halfVector, viewDir, lightDir, metallic, roughness, color.xyz, LightColour);
		}
	}

	vec3 ambient = vec3(0.02) * color.xyz;
	outLight += ambient;

	// Compute the final color
	vec3 finalColor = (vec3(outLight + emissive));

	// Determine brightness using luminance: Joey De Vries. Learn OpenGL: Learn Modern OpenGL Graphics Programming in a Step-By-Step Fashion. Kendall & Welling, 2020.
	float brightness = dot(finalColor.rgb, vec3(0.2126, 0.7152, 0.0722));
	if(brightness > 1.0)
		brightColours = vec4(finalColor.rgb, 1.0);
	else
		brightColours = vec4(0.0, 0.0, 0.0, 1.0);

	fragColor = vec4(vec3(finalColor), 1.0);
}

