
#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D renderedScene;
layout(set = 0, binding = 1) uniform sampler2D bloomPass;
layout(set = 0, binding = 2) uniform sampler2D SSR;


// Combine the deferred lighting pass and the bloom pass to produce the final output image
void main()
{
	vec4 lighting = texture(renderedScene, uv);
	vec4 bloom = texture(bloomPass, uv);
	vec4 ssr = texture(SSR, uv);

	vec4 result = lighting + ssr;

	vec3 hdrColor = result.rgb;
	vec3 ldrColor = hdrColor / (hdrColor + vec3(1.0));
	vec3 gammaCorrectedColor = pow(ldrColor, vec3(1.0 / 2.2));
	fragColor = vec4((gammaCorrectedColor.rgb), 1.0);
}