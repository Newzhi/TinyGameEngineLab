#version 330 core
layout (location = 0) in vec3 aPos;

// std140：projection 在偏移 0，view 在偏移 64，整块 128 字节。
// 块名 Matrices 要和 CPU 侧 glGetUniformBlockIndex 的名字一致。
// 3.3 不能写 layout(binding = 0)，绑定点在 CPU 用 glUniformBlockBinding 设置。
layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

// 每个立方体不同，继续走普通 uniform
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
