
#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D renderedScene;
layout(set = 0, binding = 1) uniform sampler2D bloomPass;
layout(set = 0, binding = 2) uniform sampler2D SSR;
layout(set = 0, binding = 3) uniform sampler2D SSAO;

// TODO: Use seperable Gaussian blur to denoise SSAO
// Spatially denoising SSAO for now.
vec3 SpatialDenoise(sampler2D inputImage)
{
    ivec2 loc = ivec2(gl_FragCoord.xy) - ivec2(2);
	vec3 total = vec3(0.0);

    vec2 texelSize = 1.0 / vec2(textureSize(inputImage, 0));
    vec3 result = vec3(0.0);
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(inputImage, uv + offset).rgb;
        }
    }

    return vec3(result / 16.0);
}

// Combine the deferred lighting pass and the bloom pass to produce the final output image
void main()
{
	vec4 lighting = texture(renderedScene, uv);
	vec4 bloom = texture(bloomPass, uv);
	vec4 ssr = texture(SSR, uv);
	vec3 ssao = SpatialDenoise(SSAO);

	lighting.rgb *= ssao;
	vec4 result = vec4(lighting + bloom + ssr);

	vec3 hdrColor = result.rgb;
	vec3 ldrColor = hdrColor / (hdrColor + vec3(1.0));
	vec3 gammaCorrectedColor = pow(ldrColor, vec3(1.0 / 2.2));
	fragColor = vec4((gammaCorrectedColor), 1.0);
}