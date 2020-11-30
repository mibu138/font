#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 uvw;

layout(location = 0) out vec3 outUvw;

void main()
{
    gl_Position = vec4(pos, 1.0);
    outUvw = uvw;
}
