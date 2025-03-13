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


layout(set = 0, binding = 1) uniform SSRSettings
{
    int MaxSteps;
    int BinarySearchIterations;
    float MaxDistance;
    float thickness;
	float StepSize;
}ssr;

layout(set = 0, binding = 2) uniform sampler2D depthBuffer;
layout(set = 0, binding = 3) uniform sampler2D renderedScene;
layout(set = 0, binding = 4) uniform sampler2D metallicRoughness;
layout(set = 0, binding = 5) uniform sampler2D normalsTexture;

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

vec4 NaiveScreenSpaceReflections()
{
	int STEPS = ssr.MaxSteps;
	float MAX_DISTANCE = ssr.MaxDistance;
	float stepSize = MAX_DISTANCE / float(STEPS);

	vec4 WorldPos = DepthToPosition(uv);
	vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));

	vec3 camDir = normalize(WorldPos.xyz - ubo.cameraPosition.xyz);
	vec3 worldReflectionDir = normalize(reflect(camDir, WorldNormal.xyz));

	vec3 RayPos = WorldPos.xyz;
	vec3 RayStep = worldReflectionDir * stepSize;

	RayPos += RayStep;
	vec4 color = vec4(0,0,0,0);
	for(int i = 0; i < STEPS; i++)
	{
		// Get the position of the ray in screen-space
		vec4 projectedCoords = ubo.projection * ubo.view * vec4(RayPos.xyz, 1.0);
		projectedCoords.xyz /= projectedCoords.w;
		projectedCoords.xy = projectedCoords.xy * 0.5 + 0.5;

		// check if outside view-frustum
		if(projectedCoords.x < 0.0 || projectedCoords.x > 1.0 || projectedCoords.y < 0.0 || projectedCoords.y > 1.0)
		{
			// fade out based on how far along the ray we are
			float fade = smoothstep(0.0, 1.0, float(i) * stepSize / MAX_DISTANCE);
			return vec4(0.0, 0.0, 0.0, 1.0);
		}

		float rayDepth = projectedCoords.z;
		float depth = texture(depthBuffer, projectedCoords.xy).x;

		if((rayDepth - depth) > 0.0 && (rayDepth - depth) < ssr.thickness)
		{
			// We hit geometry

			vec3 hitPos = RayPos;
			vec3 prevPos = RayPos - RayStep;
			for (int j = 0; j < ssr.BinarySearchIterations; j++) { // 4 iterations for refinement
				vec3 midPos = (hitPos + prevPos) * 0.5;
				vec4 midCoords = ubo.projection * ubo.view * vec4(midPos, 1.0);
				midCoords.xyz /= midCoords.w;
				midCoords.xy = midCoords.xy * 0.5 + 0.5;
				float midDepth = texture(depthBuffer, midCoords.xy).x;
				if (midDepth < midCoords.z) {
					hitPos = midPos;
				} else {
					prevPos = midPos;
				}
			}
			projectedCoords = ubo.projection * ubo.view * vec4(hitPos, 1.0);
			projectedCoords.xyz /= projectedCoords.w;
			projectedCoords.xy = projectedCoords.xy * 0.5 + 0.5;

			float NdotR = max(dot(-camDir, worldReflectionDir), 0.0);
			color = texture(renderedScene, projectedCoords.xy);

			vec2 center = vec2(0.5, 0.5);
			float fadeStart = 0.3;
			float fadeEnd = 0.5;

			float dist = length(projectedCoords.xy - center);

			float fadeFactor = smoothstep(fadeEnd, fadeStart, dist);
			return mix(color * fadeFactor, vec4(0), NdotR);
			//return mix(color.rgb, vec3(0,0,0), NdotR); // if its closer to 1, we get less reflection since its aligned with camera
		}

		RayPos += RayStep;
	}

	return vec4(0.0, 0.0, 0.0, 1.0);
}

vec4 DepthToWorldPos(vec2 uv)
{
	float depth = texture(depthBuffer, uv).x;
	vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 viewSpace = inverse(ubo.projection) * clipSpace;
	viewSpace.xyz /= viewSpace.w;

	vec4 worldSpace = inverse(ubo.view) * vec4(viewSpace.xyz, 1.0);
	return vec4(worldSpace.xyz, 1.0);
}

vec4 DepthToWorldNormal(vec2 uv)
{
	vec4 worldPosition = DepthToWorldPos(uv);
	vec3 n = normalize(cross(dFdx(worldPosition.xyz), dFdy(worldPosition.xyz)));
	n *= -1;
	return vec4(n, 1.0);
}

vec3 ScreenSpaceReflectionsOriginal()
{
	const vec2 texSize = textureSize(depthBuffer, 0);
    int MAX_DISTANCE = int(ssr.MaxDistance);
    // World
    vec3 WorldPos = DepthToWorldPos(uv).xyz; // Get the world position of the current fragment
    vec3 camDir = normalize(WorldPos - ubo.cameraPosition.xyz); // Get the camera direction

    vec3 WorldNormal = DepthToWorldNormal(uv).xyz; // Get the world normal of the current fragment
    vec3 worldReflectionDir = normalize(reflect(camDir, WorldNormal.xyz)); // Get the reflection direction

    vec3 WorldSpaceBegin = WorldPos.xyz; // The start of the ray in world-space
    vec3 worldSpaceEnd   = WorldSpaceBegin.xyz + worldReflectionDir * MAX_DISTANCE; // The end of the ray in world-space

    // Get the start and end of the ray in screen-space
    vec4 start = ubo.projection * ubo.view * vec4(WorldSpaceBegin.xyz, 1.0);
    start.xyz /= start.w;
    start.xy = start.xy * 0.5 + 0.5;
    start.xy *= texSize;

    vec4 end = ubo.projection * ubo.view * vec4(worldSpaceEnd, 1.0);
    end.xyz /= end.w;
    end.xy = end.xy * 0.5 + 0.5;
    end.xy *= texSize;

    const int dx = int(end.x) - int(start.x);
    const int dy = int(end.y) - int(start.y);

    // I need to know which direction I should be going in depending on whether x or y is larger
    int stepDir = max(abs(dx), abs(dy));
    stepDir = (stepDir + 1) / 2;
    // Now we need the reciprocal of the stepDir to get the step size to move in either x or y
    const float stepRCP = 1.0 / float(stepDir);

    // This is how much we move in both x and y per step
    const float x_incr = float(dx) * stepRCP;
    const float y_incr = float(dy) * stepRCP;

    // We begin at the start screen-space position
    float x = float(start.x);
    float y = float(start.y);

    // We want to keep moving forward the end point in screen-space until we reach the end determined by stepDir
    for(int i = 0; i < ssr.MaxSteps; i++)
    {
        x += x_incr;
        y += y_incr;
        // We now need to linearly interpolate the depth of the current position
        const float dt = float(i) / float(stepDir); // This is the percentage of the way we are through the ray
        const float z = mix(start.z, end.z, dt); // This is the interpolated depth at the current position

        // Get the depth at the current screen-space position
        const float depth = (texelFetch(depthBuffer, ivec2(x, y), 0).x);

		vec2 screenSpace = vec2(x,y) / texSize;

		if(screenSpace.x < 0.0 || screenSpace.x > 1.0 || screenSpace.y < 0.0 || screenSpace.y > 1.0)
		{
			break;
		}
		// TODO:
		// Not check if this is going out of bounds or not which could be why there is stretching

        // Compare the depths to see if we hit geometry
        if(depth < z - ssr.thickness)
        {
            // We hit geometry, sample the color
            return texelFetch(renderedScene, ivec2(x, y), 0).rgb;
        }
    }

    return vec3(0, 0, 0);
}

void main() {

	vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));

	//fragColor = vec4(WorldNormal, 1.0);
	bool hasSSR = texture(metallicRoughness, uv).r < 0.9;
	if(hasSSR)
		fragColor = vec4(NaiveScreenSpaceReflections());
	else
		fragColor = vec4(0);
}