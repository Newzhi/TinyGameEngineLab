#version 330 core
// 世界坐标交给 GS，由它乘六个面的 light-space 矩阵。
layout (location = 0) in vec3 aPos;

uniform mat4 model;

void main()
{
    gl_Position = model * vec4(aPos, 1.0);
}
