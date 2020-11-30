#version 460

layout(location = 0) in  vec3 uvw;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform usampler2D image;

void main()
{
    vec2 st = uvw.st;
    uint val = texture(image, st).r;
    float c = float(val) / 255.0;
    outColor = vec4(0, .1, .7, c);
    //outColor = vec4(uvw, 1);
}
