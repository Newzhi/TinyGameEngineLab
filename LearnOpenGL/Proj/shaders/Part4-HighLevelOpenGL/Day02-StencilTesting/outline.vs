#version 330 core
// 轮廓 / 地板共用：只需要位置 + MVP，输出纯色。
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
