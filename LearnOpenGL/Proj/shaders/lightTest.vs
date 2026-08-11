#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;   // 片元世界坐标，供漫反射算光方向
out vec3 Normal;    // 世界空间法线

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    // 法线矩阵：处理非等比缩放；本 Demo model 无缩放也可简化为 mat3(model)
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
