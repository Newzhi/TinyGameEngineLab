#version 330 core
// 立方体：位置 + 顶点色。MSAA 发生在光栅化阶段，着色器无需改。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 Color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    Color = aColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
