#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;
// mat4 占 4 个 location：3 / 4 / 5 / 6。会盖掉 Mesh 里的切线、副切线，本 Demo 只用漫反射。
layout (location = 3) in mat4 aInstanceMatrix;

out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;
    // 每颗小行星自己的 model 来自实例缓冲，不再走 uniform
    gl_Position = projection * view * aInstanceMatrix * vec4(aPos, 1.0);
}
