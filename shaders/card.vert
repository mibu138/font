#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 uvw;

layout(location = 0) out vec3 outUvw;
layout(location = 1) out vec3 outColor;

layout(push_constant) uniform PushConst {
    vec4 color;
} pushConst;

void main()
{
    gl_Position = vec4(pos, 1.0);
    outUvw = uvw;
    outColor = pushConst.color.rgb;
}
