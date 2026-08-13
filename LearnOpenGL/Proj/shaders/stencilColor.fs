#version 330 core
out vec4 FragColor;

// 画面 A / 画面 B 只是两种纯色，用来看清「模板通过画什么、不通过画什么」。
uniform vec3 uColor;

void main()
{
    FragColor = vec4(uColor, 1.0);
}
