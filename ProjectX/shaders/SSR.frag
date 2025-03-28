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

vec3 sampleGGXVNDF(vec3 N, float roughness, vec2 Xi) {

	float alpha = roughness * roughness; // Convert roughness to GGX alpha

    // Transform Xi to spherical coordinates
    float phi = 2.0 * 3.141592653589793 * Xi.x; // Uniform azimuthal angle
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha * alpha - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Compute the half-vector in tangent space
    vec3 H_tangent = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Transform from tangent space to world space
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangentX = normalize(cross(up, N));
    vec3 tangentY = cross(N, tangentX);

    return normalize(tangentX * H_tangent.x + tangentY * H_tangent.y + N * H_tangent.z);

}

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 getRandomXi(vec2 fragCoord) {
    vec2 Xi;
    Xi.x = rand(fragCoord);
    Xi.y = rand(fragCoord + vec2(1.0, 2.0)); // Offset to get a different value
    return Xi;
}

vec4 NaiveScreenSpaceReflections()
{
	int STEPS = ssr.MaxSteps;
	float MAX_DISTANCE = ssr.MaxDistance;
	float stepSize = MAX_DISTANCE / float(STEPS);

	vec4 WorldPos = DepthToPosition(uv);
	vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));

	vec3 camDir = normalize(WorldPos.xyz - ubo.cameraPosition.xyz);
	vec3 reflectionDirection = sampleGGXVNDF(WorldNormal, ssr.thickness, getRandomXi(gl_FragCoord.xy));

	vec3 worldReflectionDir = (reflectionDirection);

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

		if((rayDepth - depth) > 0.0 && (rayDepth - depth) < 0.001)
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

			//float fadeFactor = smoothstep(fadeEnd, fadeStart, dist);

			// Convert to NDC (-1 to 1 range)
			vec2 hitPixelNDC = projectedCoords.xy * 2.0 - 1.0;
			const float blendScreenEdgeFade = 5.0f;

			// Compute edge vignette (similar to CalculateEdgeVignette)
			vec2 vignette = clamp(abs(hitPixelNDC) * blendScreenEdgeFade - (blendScreenEdgeFade - 1.0), 0.0, 1.0);
			float fadeFactor = clamp(1.0 - dot(vignette, vignette), 0.0, 1.0);

			return color;

			//return mix(color.rgb, vec3(0,0,0),	 NdotR); // if its closer to 1, we get less reflection since its aligned with camera
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


float ScreenSpaceShadows()
{
	int STEPS = ssr.MaxSteps;
	float MAX_DISTANCE = ssr.MaxDistance;
	float stepSize = MAX_DISTANCE / float(STEPS);

	vec4 WorldPos = DepthToPosition(uv);
	vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));
	vec3 camDir = normalize(WorldPos.xyz - ubo.cameraPosition.xyz);

	// To compute screen-space shadows we want to shoot a ray, we want to shooto a ray towards
	// the light source and see if it hits something on the way there? we check this if raydepth > depthbuffer && < 0.001


    // Don't have the light position here as a uniform buffer
    // Using a place holder directional light direction
	vec3 directional_light_dir = vec3(0, 1, 0);
	vec3 position_to_light_dir = normalize(directional_light_dir - WorldPos.xyz);

	vec3 RayPos = WorldPos.xyz;
	RayPos += position_to_light_dir * stepSize * rand(uv); // offset to prevent self-intersection

	#pragma unroll
	for(int i = 0; i < STEPS; i++)
	{
		vec4 projectedCoords = ubo.projection * ubo.view * vec4(RayPos.xyz, 1.0);
		projectedCoords.xyz /= projectedCoords.w;
		projectedCoords.xy = projectedCoords.xy * 0.5 + 0.5;

		float rayDepth = projectedCoords.z;
		float sceneDepth = texture(depthBuffer, projectedCoords.xy).x;

		bool intersected = ((rayDepth - sceneDepth) > 0.0 && (rayDepth - sceneDepth) < ssr.thickness);
		if(intersected)
		{
			return 0.0;
		}

		RayPos += position_to_light_dir * stepSize * rand(uv);
	}

	return 1.0;
}

// "screen-space" refers to "pixel-space" i.e. the screen-space position of a pixel not in the range 0-1
vec3 ScreenSpaceReflections()
{
    const vec2 texSize = textureSize(depthBuffer, 0); // Should use ubo.viewportSize
    const float MAX_DISTANCE = ssr.MaxDistance;

    // World-space setup
    vec3 WorldPos = DepthToPosition(uv).xyz;
    vec3 camDir = normalize(WorldPos - ubo.cameraPosition.xyz);
    vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));;
    vec3 worldReflectionDir = normalize(reflect(camDir, WorldNormal));

    vec3 WorldSpaceBegin = WorldPos;
    vec3 worldSpaceEnd = WorldSpaceBegin + worldReflectionDir * MAX_DISTANCE;

    // Project to screen space
    vec4 start = ubo.projection * ubo.view * vec4(WorldSpaceBegin, 1.0);
    start.xyz /= start.w;
    start.xy = start.xy * 0.5 + 0.5;
    start.xy *= texSize;

    vec4 end = ubo.projection * ubo.view * vec4(worldSpaceEnd, 1.0);
    end.xyz /= end.w;
    end.xy = end.xy * 0.5 + 0.5;
    end.xy *= texSize;

    // Start and End setup, determine step direction
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    int stepDir = max(abs(int(dx)), abs(int(dy)));

	// Early exit if start and end are the same. We don't need to traverse the ray
    if (stepDir == 0) {
        return vec3(0.0);
    }

    float stepRCP = 1.0 / float(stepDir);
    float x_incr = dx * stepRCP;
    float y_incr = dy * stepRCP;

    float x = start.x;
    float y = start.y;

    // Traverse the ray
    for (int i = 0; i < stepDir; i++) {

        x += x_incr;
        y += y_incr;

        // Check if x,y are outside the bounds of pixel-space-screen-space
        if (x < 0.0 || x >= texSize.x || y < 0.0 || y >= texSize.y) {
            break;
        }

        // Interpolate depth
        float t = float(i) / float(stepDir);
        float z = mix(start.z, end.z, t);
        float depth = texelFetch(depthBuffer, ivec2(x, y), 0).x;

        // Depth test to determine hit
        float depthDiff = z - depth;
        if (depthDiff > 0.0 && depthDiff < ssr.thickness) { // Tighter range

			// Sample the colour at the hit point
			vec3 colour = texelFetch(renderedScene, ivec2(x, y), 0).rgb;

			// Screen-fading at edges
			vec2 hitPixelNDC = (vec2(x,y) / texSize) * 2.0 - 1.0; // get in NDC [-1, 1]
			const float blendScreenEdgeFade = 5.0f;
			vec2 vignette = clamp(abs(hitPixelNDC) * blendScreenEdgeFade - (blendScreenEdgeFade - 1.0), 0.0, 1.0);
			float screenFade = clamp(1.0 - dot(vignette, vignette), 0.0, 1.0);

			// Rays which point towards the camera have less contribution (likely hitting the back of a surface)
			float NdotR = max(dot(normalize(-camDir), normalize(worldReflectionDir)), 0.0);
			float TowardsCameraVisbility = clamp(1.0 - NdotR, 0.0, 1.0);

			// Fade the ray based on the distance the ray has travelled
			float DistanceTravelled = 1.0 - clamp(float(i) / float(stepDir), 0.0, 1.0);
			return colour * TowardsCameraVisbility * DistanceTravelled * screenFade;
        }
    }

    return vec3(0.0);
}


const float PI = 3.141592653589793;
const float min_roughness = 0.002;
const float GGX_IMPORTANCE_SAMPLE_BIAS = 0.1;

// GGX Importance Sampling (from Brian Karis - UE4)
vec4 ImportanceSampleGGX(vec2 Xi, float Roughness) {
    Roughness = clamp(Roughness, min_roughness, 1.0);
    float m = Roughness * Roughness;
    float m2 = m * m;

    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (m2 - 1.0) * Xi.y));
    float SinTheta = sqrt(max(1e-5, 1.0 - CosTheta * CosTheta));

    vec3 H = vec3(SinTheta * cos(Phi), SinTheta * sin(Phi), CosTheta);

    float d = (CosTheta * m2 - CosTheta) * CosTheta + 1.0;
    float D = m2 / (PI * d * d);
    float pdf = D * CosTheta;

    return vec4(H, pdf);
}

// Orthonormal Basis (Duff et al. 2017)
mat3 GetTangentBasis(vec3 TangentY) {
    float Sign = TangentY.y >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (Sign + TangentY.y);
    float b = TangentY.x * TangentY.z * a;

    vec3 TangentX = vec3(1.0 + Sign * a * TangentY.x * TangentY.x, -Sign * TangentY.x, b);
    vec3 TangentZ = vec3(b, -TangentY.z, Sign + a * TangentY.z * TangentY.z);

    return mat3(TangentX, TangentY, TangentZ);
}

// Sample Disk for VNDF
vec2 SampleDisk(vec2 Xi) {
    float theta = 2.0 * PI * Xi.x;
    float radius = sqrt(Xi.y);
    return radius * vec2(cos(theta), sin(theta));
}

// GGX VNDF Importance Sampling (Heitz 2017)
vec4 ImportanceSampleVisibleGGX(vec2 diskXi, float roughness, vec3 V) {
    roughness = clamp(roughness, min_roughness, 1.0);
    float alphaRoughness = roughness * roughness;
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    vec3 Vh = normalize(vec3(alphaRoughness * V.x, V.y, alphaRoughness * V.z));

    vec3 tangent0 = (abs(Vh.y) < 0.9999) ? normalize(cross(vec3(0, 1, 0), Vh)) : vec3(1, 0, 0); // Y-up
    vec3 tangent1 = cross(Vh, tangent0);

    vec2 p = diskXi;
    float s = 0.5 + 0.5 * Vh.y;
    p.y = (1.0 - s) * sqrt(1.0 - p.x * p.x) + s * p.y;

    vec3 H = p.x * tangent0 + p.y * tangent1 + sqrt(clamp(1.0 - dot(p, p), 0.0, 1.0)) * Vh;
    H = normalize(vec3(alphaRoughness * H.x, max(0.0, H.y), alphaRoughness * H.z));

    float NdotV = V.y;
    float NdotH = H.y;
    float VdotH = dot(V, H);

    float f = (NdotH * alphaRoughnessSq - NdotH) * NdotH + 1.0;
    float D = alphaRoughnessSq / (PI * f * f);

    float SmithGGXMasking = 2.0 * NdotV / (sqrt(NdotV * (NdotV - NdotV * alphaRoughnessSq) + alphaRoughnessSq) + NdotV);

    float PDF = SmithGGXMasking * VdotH * D / NdotV;

    return vec4(H, PDF);
}

// Generate reflection direction using GGX
vec4 ReflectionDir_GGX(vec3 V, vec3 N, float roughness, vec2 random2) {
    roughness = clamp(roughness, min_roughness, 1.0);
    vec4 H;
    vec3 L;

    if (roughness > 0.05) {
        mat3 tangentBasis = GetTangentBasis(N);
        vec3 tangentV = tangentBasis * V;
        vec2 Xi = random2;
        Xi.y = mix(Xi.y, 0.0, GGX_IMPORTANCE_SAMPLE_BIAS);
        H = ImportanceSampleVisibleGGX(SampleDisk(Xi), roughness, tangentV);
        H.xyz = tangentBasis * H.xyz;
        L = reflect(-V, H.xyz);
    } else {
        H = vec4(N, 1.0);
        L = reflect(-V, H.xyz);
    }

    return vec4(L, H.w);
}

vec4 NaiveScreenSpaceReflectionsS()
{
    int STEPS = ssr.MaxSteps;
    float MAX_DISTANCE = ssr.MaxDistance;
    float stepSize = MAX_DISTANCE / float(STEPS);

    vec4 WorldPos = DepthToPosition(uv);
    vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));
    vec3 camDir = normalize(WorldPos.xyz - ubo.cameraPosition.xyz);

    const int NUM_SAMPLES = 1;  // Number of rays
    vec4 accumulatedColor = vec4(0.0);
    float accumulatedWeight = 0.0;

    for (int sampleIdx = 0; sampleIdx < NUM_SAMPLES; sampleIdx++)
    {
        vec2 Xi = getRandomXi(gl_FragCoord.xy + vec2(sampleIdx));
        vec4 reflectionData = ReflectionDir_GGX(-camDir, WorldNormal, ssr.thickness, Xi);
        vec3 worldReflectionDir = normalize(reflectionData.xyz);
        float PDF = reflectionData.w;

        vec3 RayPos = WorldPos.xyz + worldReflectionDir * stepSize;
        vec3 RayStep = worldReflectionDir * stepSize;

        for (int i = 0; i < STEPS; i++)
        {
            vec4 projectedCoords = ubo.projection * ubo.view * vec4(RayPos.xyz, 1.0);
            projectedCoords.xyz /= projectedCoords.w;
            projectedCoords.xy = projectedCoords.xy * 0.5 + 0.5;

            if (projectedCoords.x < 0.0 || projectedCoords.x > 1.0 || projectedCoords.y < 0.0 || projectedCoords.y > 1.0)
                break;

            float rayDepth = projectedCoords.z;
            float depth = texture(depthBuffer, projectedCoords.xy).x;

            if ((rayDepth - depth) > 0.0 && (rayDepth - depth) < 0.001)
            {
                vec4 hitColor = texture(renderedScene, projectedCoords.xy);
                float weight = max(dot(WorldNormal, worldReflectionDir), 0.0001) / (PDF + 0.0001);

                accumulatedColor += hitColor * weight;
                accumulatedWeight += weight;
                break;
            }

            RayPos += RayStep;
        }
    }

    if (accumulatedWeight > 0.0)
        return accumulatedColor / accumulatedWeight;

    return vec4(0.0, 0.0, 0.0, 1.0);
}

vec4 NaiveScreenSpaceReflections3()
{
    int STEPS = ssr.MaxSteps;
    float MAX_DISTANCE = ssr.MaxDistance;
    float stepSize = MAX_DISTANCE / float(STEPS);

    vec4 WorldPos = DepthToPosition(uv);
    vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));
    vec3 camDir = normalize(WorldPos.xyz - ubo.cameraPosition.xyz);

    const int NUM_SAMPLES = ssr.BinarySearchIterations;  // Number of rays
    vec4 accumulatedColor = vec4(0.0);
    float accumulatedWeight = 0.0;

    for (int sampleIdx = 0; sampleIdx < NUM_SAMPLES; sampleIdx++)
    {
        vec2 Xi = getRandomXi(gl_FragCoord.xy + vec2(sampleIdx));
        vec4 reflectionData = ReflectionDir_GGX(-camDir, WorldNormal, ssr.thickness, Xi);
        vec3 worldReflectionDir = normalize(reflectionData.xyz);
        float PDF = reflectionData.w;

        vec3 RayPos = WorldPos.xyz + worldReflectionDir * stepSize;
        vec3 RayStep = worldReflectionDir * stepSize * rand(uv);

        for (int i = 0; i < STEPS; i++)
        {
            vec4 projectedCoords = ubo.projection * ubo.view * vec4(RayPos.xyz, 1.0);
            projectedCoords.xyz /= projectedCoords.w;
            projectedCoords.xy = projectedCoords.xy * 0.5 + 0.5;

            if (projectedCoords.x < 0.0 || projectedCoords.x > 1.0 || projectedCoords.y < 0.0 || projectedCoords.y > 1.0)
                break;

            float rayDepth = projectedCoords.z;
            float depth = texture(depthBuffer, projectedCoords.xy).x;

            if ((rayDepth - depth) > 0.0 && (rayDepth - depth) < 0.001)
            {
                vec4 hitColor = texture(renderedScene, projectedCoords.xy);
                float weight = max(dot(WorldNormal, worldReflectionDir), 0.0001) / (PDF + 0.0001);

                accumulatedColor += hitColor * weight;
                accumulatedWeight += weight;
                break;
            }

            RayPos += RayStep;
        }
    }

    if (accumulatedWeight > 0.0)
        return accumulatedColor / accumulatedWeight;

    return vec4(0.0, 0.0, 0.0, 1.0);
}

void main() {

	vec3 WorldNormal = normalize((texture(normalsTexture, uv).xyz * 2.0 - 1.0));

	//fragColor = vec4(WorldNormal, 1.0);
	fragColor = vec4(vec3(ScreenSpaceShadows()), 1.0);
//	bool hasSSR = texture(metallicRoughness, uv).r < 0.9;
//	if(hasSSR)
//		fragColor = vec4(NaiveScreenSpaceReflections());
//	else
//		fragColor = vec4(0);
}