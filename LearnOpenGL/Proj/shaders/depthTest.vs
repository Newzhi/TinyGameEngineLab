#version 330 core

// 深度可视化只关心顶点位置。模型 Mesh 与手写箱子的 location 0 都是位置，
// 因此同一份 shader 可以画当前场景里的所有不透明几何体。
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 与正常渲染完全相同的 MVP 变换。
    // 透视除法和视口变换之后，GPU 会得到 [0, 1] 范围的片段深度。
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
