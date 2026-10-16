#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    // 立方体中心在原点时，顶点位置就是从中心指出的方向 → 直接当 cubemap 采样向量
    TexCoords = aPos;

    vec4 pos = projection * view * vec4(aPos, 1.0);
    // 透视除法会做 xyz / w。把 z 写成 w，除完 z = 1.0（最远），
    // 这样后画天空盒时只填「深度缓冲还是 1.0 / 没有更近物体」的像素。
    gl_Position = pos.xyww;
}
