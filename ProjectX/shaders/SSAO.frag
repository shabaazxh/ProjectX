#version 450
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 uv;
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

layout(set = 0, binding = 1) uniform SSAOSettings
{
	int NumDirections;
	int NumSteps;
	float Radius;
	float StepSize;
	float k;
	float sigma;
	float time;
}ssao;

layout(set = 0, binding = 2) uniform sampler2D depthBuffer;
layout(set = 0, binding = 3) uniform sampler2D normalsTexture;

#define PI 3.14159265359

vec4 DepthToPosition(vec2 uv)
{
	float depth = texture(depthBuffer, uv).x;
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpace = inverse(ubo.projection) * clipSpace;
	viewSpace.xyz /= viewSpace.w;

	vec4 worldPos = inverse(ubo.view) * vec4(viewSpace.xyz, 1.0);
	return vec4(worldPos.xyz, 1.0);
}

vec4 DepthToNormal(vec2 uv)
{
	float depth = texture(depthBuffer, uv).x;

	vec4 viewPosition = DepthToPosition(uv);

	vec3 n = normalize(cross(dFdx(viewPosition.xyz), dFdy(viewPosition.xyz)));
	n *= -1;

	return vec4(n, 1.0);
}

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 RotateDirectionAngle(vec2 direction, vec2 noise)
{
    // contruct a rotation matrix to rotate the direction
    mat2 rotationMatrix = mat2(vec2(noise.x, -noise.y), vec2(noise.y, noise.x));
    return direction * rotationMatrix;
}

vec2 snapToTexelCenter(vec2 texCoord, vec2 texSize) {
    vec2 texelSize = 1.0 / texSize;
    return floor(texCoord / texelSize) * texelSize + texelSize * 0.5;
}

vec4 GetJitter()
{
    //return texture(generatedNoiseTex, (gl_FragCoord.xy / 4.0));

	return vec4(rand(uv));
}

float SSAO()
{
	int NUMBER_OF_SAMPLING_DIRECTIONS = ssao.NumDirections;//22;
	float NUMBER_OF_STEPS = ssao.NumSteps;//4;
	float STEP = ssao.StepSize;//0.002;
    float RADIUS = ssao.Radius;//2.2;

	float ao = 0.0;
	float occlusion = 0.0;

	// get the current fragments world pos and normal
	vec4 WorldPos = DepthToPosition(uv);
	vec4 WorldNormal = normalize(vec4(texture(normalsTexture, uv)));

	// convert to view-space
	vec4 viewSpacePos = ubo.view * vec4(WorldPos.xyz, 1.0);
	vec4 viewSpaceNormal = normalize(ubo.view * vec4(WorldNormal.xyz, 0.0));

	float samplingDisk = ((2 * PI * RADIUS)) / NUMBER_OF_SAMPLING_DIRECTIONS; // integral says its from 0-pi not from 0-2pi

    vec4 Rand = GetJitter();

	for(int i = 0; i < NUMBER_OF_SAMPLING_DIRECTIONS; i++)
	{
		float samplingAngle = i * samplingDisk; // tan(30.0 * PI / 180.0)
		vec2 samplingDirection = RotateDirectionAngle(vec2(cos(samplingAngle), sin(samplingAngle)), Rand.xy);

		float tangentAngle = acos(dot(vec3(samplingDirection, 0.0), viewSpaceNormal.xyz)) - (0.5 * PI) + 0.3;
		float horizonAngle = tangentAngle;

		vec3 lastDifference = vec3(0);

		for(int j = 0; j < NUMBER_OF_STEPS; j++)
		{
			// get the screen space position of the point
            vec2 stepScreenSpace = uv + (Rand.z + float(j+1)) * STEP * samplingDirection;

			stepScreenSpace = snapToTexelCenter(stepScreenSpace, ubo.viewportSize);

			// depth buffer z value stored at the location the ray is
			float steppedLocationZ = texture(depthBuffer, stepScreenSpace.st).x;

			// complete screen space point
			vec3 steppedLocationPosition = vec3(stepScreenSpace, steppedLocationZ);
			// NDC
			vec3 steppedPositionNDC = vec3((2.0 * steppedLocationPosition.xy) - 1.0, steppedLocationPosition.z);
			// unproject
			vec4 steppedPositionUnproj = inverse(ubo.projection) * vec4(steppedPositionNDC, 1.0);
			// back to view-space
			vec3 viewSpaceSteppedPosition = vec3(steppedPositionUnproj.xyz / steppedPositionUnproj.w);

            vec3 diff = viewSpaceSteppedPosition.xyz - viewSpacePos.xyz;

            float t = length(diff) / RADIUS;
			float u = 0.10;

			float contribution = (u * t) / (max(u, t) * max(u, t));
			//ao += (max(0.0, contribution * NdotL)) / (dot(direction,direction) + 0.0001);

            bool inRadius = length(diff) < RADIUS;
            occlusion = inRadius ?
            (viewSpaceSteppedPosition.z > viewSpacePos.z ?
            occlusion + max(0.0, max(dot(viewSpaceNormal.xyz, normalize(diff)) * contribution, 0.0)) / (dot(normalize(diff), normalize(diff) + 0.0001))
            : occlusion)
            : occlusion;
		}
	}

    float k = ssao.k;
    float sigma = ssao.sigma;
    occlusion *= (2.0 * sigma) / float(NUMBER_OF_SAMPLING_DIRECTIONS);
    occlusion = pow(max(0.0, 1.0 - occlusion), k);

    ao = occlusion;
    return ao;

	//ao = occlusion / float(NUMBER_OF_SAMPLING_DIRECTIONS) * SSAOSettings.k;
	//return 1.0 - ao;
}

void main() {

	fragColor = vec4(vec3(SSAO()), 1.0);
}