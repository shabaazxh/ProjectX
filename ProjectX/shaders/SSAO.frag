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
layout(set = 0, binding = 4) uniform sampler2D NoiseTexture;

#define PI 3.14159265359

vec4 DepthToPosition(vec2 uv, float depth)
{
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpace = inverse(ubo.projection) * clipSpace;
	viewSpace.xyz /= viewSpace.w;

	return vec4(viewSpace.xyz, 1.0);
}

vec4 DepthToNormal(vec2 uv, float depth)
{
	vec4 viewPosition = DepthToPosition(uv, depth);
	vec3 n = (cross(dFdx(viewPosition.xyz), dFdy(viewPosition.xyz)));
	n *= -1;
	return vec4(normalize(n), 1.0);
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
    return texture(NoiseTexture, (gl_FragCoord.xy / 4.0));
}

float NUMBER_OF_SAMPLING_DIRECTIONS = ssao.NumDirections;
float STEP = ssao.StepSize; //0.04
float NUMBER_OF_STEPS = ssao.NumSteps;
float TANGENT_BIAS = 0.523599;
float HalfPI = 0.5 * PI;
float TAU = 2 * PI;
float RADIUS = ssao.Radius;
float RADIUS2 = RADIUS * RADIUS;
float invNumDirections = 1.0 / NUMBER_OF_SAMPLING_DIRECTIONS;

float HBAO()
{
	float ao = 0.0;
	float occlusion = 0.0;

	float fragmentDepth = texture(depthBuffer, uv).x;
    vec4 normal = DepthToNormal(uv, fragmentDepth);
	normal.y = -normal.y;
    vec3 viewPosition = DepthToPosition(uv, fragmentDepth).xyz;

    float samplingDiskDirection = TAU / NUMBER_OF_SAMPLING_DIRECTIONS;
    vec4 Rand = GetJitter();

	// vec3(0,0,0) is camera position in view space
	// float centerDepth = distance(vec3(0,0,0), viewPosition.xyz);

    for(int i = 0; i < NUMBER_OF_SAMPLING_DIRECTIONS; i++) {

        float samplingDirectionAngle = i * samplingDiskDirection;
        vec2 samplingDirection = RotateDirectionAngle(vec2(cos(samplingDirectionAngle), sin(samplingDirectionAngle)), Rand.xy);

        float tangentAngle = acos(dot(vec3(samplingDirection, 0.0), normal.xyz)) - (HalfPI) + TANGENT_BIAS;
        float horizonAngle = tangentAngle;

        vec3 LastDifference = vec3(0);
        for(int j = 0; j < NUMBER_OF_STEPS; j++){

            vec2 stepForward = (Rand.z + float(j+1)) * STEP * samplingDirection;
            vec2 stepPosition = uv + stepForward;
			stepPosition = snapToTexelCenter(stepPosition, ubo.viewportSize);

			float depthAtPosition = texture(depthBuffer, stepPosition).x;
			vec3 sampleViewPos = DepthToPosition(stepPosition, depthAtPosition).xyz;
			vec3 diff = sampleViewPos - viewPosition;

			float diffLenghSq = dot(diff, diff);

			bool inRadius = diffLenghSq < RADIUS2;
			horizonAngle = inRadius ? max(horizonAngle, atan(diff.z / length(diff.xy))) : horizonAngle;
			LastDifference = inRadius ? diff : LastDifference;
        }

        float norm = length(LastDifference) / RADIUS;
        float attenuation = 1.0 - (norm * norm);
		float sinHorizon = sin(horizonAngle);
		float sinTangent = sin(tangentAngle);

        occlusion = clamp(attenuation * (sinHorizon - sinTangent), 0.0, 1.0);
        ao += 1.0 - occlusion * ssao.sigma; // could apply intensity here
    }

    ao *= invNumDirections;
    return ao;
}

void main() {

	fragColor = vec4(vec3(HBAO()), 1.0);
}