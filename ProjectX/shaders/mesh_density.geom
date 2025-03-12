#version 450

layout (triangles) in;
layout (line_strip, max_vertices = 4) out;

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

layout(push_constant) uniform Push
{
    mat4 ModelMatrix;
    uint dTextureID; // diffuse 
    uint mTextureID; // metalness
    uint rTextureID; // roughness
    uint eTextureID; // emissive
}pc;

layout(location = 0) in vec2 uv[];
layout(location = 1) in vec3 WorldNormal[];
layout(location = 2) in vec4 WorldPosition[];

layout(location = 0) out vec3 outColor;

vec2 viewportSize = vec2(ubo.viewportSize.x, ubo.viewportSize.y);
vec2 texelSize = 1.0 / viewportSize;

void main(void)
{
    // View-space positions of the triangle vertices
    vec3 viewA = gl_in[0].gl_Position.xyz;
    vec3 viewB = gl_in[1].gl_Position.xyz;
    vec3 viewC = gl_in[2].gl_Position.xyz;

    // project incoming positions 
    vec4 projA = ubo.projection * vec4(viewA, 1.0);
    vec4 projB = ubo.projection * vec4(viewB, 1.0);
    vec4 projC = ubo.projection * vec4(viewC, 1.0);

    // apply perspective divide
    projA /= projA.w;
    projB /= projB.w;
    projC /= projC.w;

    // get points in screen space 
    projA.xy = projA.xy * 0.5 + 0.5;
    projB.xy = projB.xy * 0.5 + 0.5;
    projC.xy = projC.xy * 0.5 + 0.5;

    vec3 A = projA.xyz;
    vec3 B = projB.xyz;
    vec3 C = projC.xyz;

    vec3 AB = B - A;
    vec3 AC = C - A;

    // triangle area 
    float area = length(cross(AB, AC)) * 0.5;

    // area of texel 
    float texelArea = texelSize.x * texelSize.y;

    // compute the bounding-box for triangle 
    float minX = min(A.x, min(B.x, C.x));
    float maxX = max(A.x, max(B.x, C.x));
    float minY = min(A.y, min(B.y, C.y));
    float maxY = max(A.y, max(B.y, C.y));

    float triangleWidth = maxX - minX;
    float triangleHeight = maxY - minY;

    vec3 color = vec3(0.0);

    // Check if the area is small but the width or height exceeds texel size
    // if the area of the triangle is small and its width and height exceed a 
    // single pixel we can be sure the triangle goes onto another pixel which will then be
    // processed which would then make this triangle exceed primitive per pixel 
    // Determine color based on the area
    if((area <= texelArea) && 
        (triangleWidth > texelSize.x || triangleHeight > texelSize.y)) {
        // If both conditions are true, use red 
        color = vec3(1.0, 0.0, 0.0); 
    } 
    // Orange is if its just small area 
    else if (area <= texelArea) {
        color = vec3(1.0, 0.5, 0.0); 
    }
    else{
        color = vec3(0, 0, 1);
    }

    for (int i = 0; i < 3; ++i) {
        outColor = color;
        gl_Position = ubo.projection * gl_in[i].gl_Position;
        EmitVertex();
    }
    gl_Position = ubo.projection * gl_in[0].gl_Position;
    EndPrimitive();
}


