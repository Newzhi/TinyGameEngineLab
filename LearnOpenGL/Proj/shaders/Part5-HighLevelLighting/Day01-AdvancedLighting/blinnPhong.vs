#version 330 core
// 地板：世界空间位置 + 法线交给片元做光照；UV 采样漫反射贴图。
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));
    // 非均匀缩放时要用法线矩阵；本 Demo 地板是单位矩阵，结果等于 aNormal。
    vs_out.Normal = mat3(transpose(inverse(model))) * aNormal;
    vs_out.TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}
