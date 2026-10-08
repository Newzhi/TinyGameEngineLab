#version 330 core
layout (location = 0) in vec2 aPos;

void main()
{
    // 四个点已经在 NDC 里，不乘 MVP，直接出现在窗口四角附近
    gl_Position = vec4(aPos, 0.0, 1.0);
    // 需要 CPU 侧 glEnable(GL_PROGRAM_POINT_SIZE)，否则点太小看不见
    gl_PointSize = 16.0;
}
