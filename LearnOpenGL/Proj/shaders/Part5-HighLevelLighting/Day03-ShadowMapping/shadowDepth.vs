#version 330 core
// 只从灯的视角做 MVP。写出的 gl_Position.z 就是阴影贴图里的「最近深度」。
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
