
#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D renderedScene;
layout(set = 0, binding = 1) uniform sampler2D bloomPass;

// Combine the deferred lighting pass and the bloom pass to produce the final output image
void main()
{
	vec4 lighting = texture(renderedScene, uv);
	vec4 bloom = texture(bloomPass, uv);

	vec4 result = lighting + bloom;
	fragColor = vec4((result.rgb), 1.0);
}