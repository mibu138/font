#version 460

layout(location = 0) in  vec2 inUv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform usampler2D image;

void main()
{
    uint val = texture(image, inUv).r;
    float c = float(val) / 255.0;
    outColor = vec4(c, c, c, 1);
}
