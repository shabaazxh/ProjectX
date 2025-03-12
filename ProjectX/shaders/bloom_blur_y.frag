#version 450
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D inputImage;
layout(scalar, set = 0, binding = 1) uniform GaussianWeights
{
    float weights[22];
    float offsets[22];
} g_weights;

// Reference: Implementation based on understanding from
// https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
// https://lisyarus.github.io/blog/posts/compute-blur.html

void main() {

    int range = 43 / 2;
    vec4 colorRes = texture(inputImage, uv) * g_weights.weights[0];
    vec2 texSize = textureSize(inputImage, 0);

    for(int i = 1; i < 11; i++)
    {
        colorRes += texture(inputImage, (vec2(uv * texSize) + vec2(0.0, g_weights.offsets[i])) / texSize) * g_weights.weights[i];
        colorRes += texture(inputImage, (vec2(uv * texSize) - vec2(0.0, g_weights.offsets[i])) / texSize) * g_weights.weights[i];
    }

    fragColor = vec4(colorRes.rgb, 1.0);
}
