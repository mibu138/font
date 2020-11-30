#version 460

layout(location = 0) in vec3 uvw;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform usampler2D image;

void main()
{
    vec2 st = uvw.st;
    uint val = texture(image, st).r;
    float c = float(val) / 255.0;
    outColor = vec4(inColor, c);
    //outColor = vec4(uvw, 1);
}
