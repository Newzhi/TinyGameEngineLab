#version 330 core
// 模板入门 Demo：顶点直接给 NDC 坐标，不再做相机 MVP，便于把注意力全放在模板上。
layout (location = 0) in vec2 aPos;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
}
