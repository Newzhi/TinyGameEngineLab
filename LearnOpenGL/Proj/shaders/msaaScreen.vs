#version 330 core
// 全屏四边形：NDC 位置直接当 gl_Position，UV 交给片元着色器采样还原后的 2D 纹理。
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
