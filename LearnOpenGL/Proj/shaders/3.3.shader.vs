#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

// MVP 三个矩阵：局部 → 世界 → 观察 → 裁剪
uniform mat4 model;       // 模型矩阵：物体自身的平移/旋转/缩放
uniform mat4 view;        // 视图矩阵：把世界坐标变换到相机坐标
uniform mat4 projection;  // 投影矩阵：把 3D 场景投影到 2D 屏幕

void main()
{
    // 从右向左读：先 model，再 view，最后 projection
    gl_Position = projection * view * model * vec4(aPos, 1.0f);
    ourColor = aColor;
    TexCoord = aTexCoord;
}