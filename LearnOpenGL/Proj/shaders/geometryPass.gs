#version 330 core

// ---------------------------------------------------------------------------
// layout 告诉驱动：这一阶段的数据按什么规则进出。
//
// 几何着色器按「整个图元」运行，所以必须声明：
//   1) 进来的是哪种图元（in）
//   2) 出去的是哪种图元、最多几个顶点（out）
// 这两行和顶点着色器里的 layout (location = 0) 是同一套关键字，
// 只是这里描述的是图元类型，不是顶点属性槽位。
// ---------------------------------------------------------------------------

// 输入：每个点图元。必须和 glDrawArrays(GL_POINTS, ...) 一致。
// 点图元只有 1 个顶点，所以 gl_in 的有效下标只有 [0]。
layout (points) in;

// 输出：还是点。max_vertices 是这一次调用最多 EmitVertex 几次。
// 直通只发 1 个，写成 1。超出的顶点会被丢掉。
layout (points, max_vertices = 1) out;

void main()
{
    // gl_in 是内建数组：上一个阶段写出的 gl_Position / gl_PointSize
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();   // 把当前 gl_Position 收进正在组装的图元
    EndPrimitive(); // 提交成输出布局说的那种图元（这里是 1 个点）
}
