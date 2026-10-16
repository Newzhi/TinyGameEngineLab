#version 330 core

layout (triangles) in;
// 三个顶点各画一条线，每条线 2 个端点，而且每条线自己 EndPrimitive
// → 最多 6 次 EmitVertex
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec3 normal;
} gs_in[];

uniform mat4 projection;

const float MAGNITUDE = 0.35;

void GenerateLine(int index)
{
    // 起点：顶点本身
    gl_Position = projection * gl_in[index].gl_Position;
    EmitVertex();
    // 终点：沿观察空间法线走出 MAGNITUDE。w=0 表示这是方向，不平移
    gl_Position = projection * (gl_in[index].gl_Position
                  + vec4(gs_in[index].normal, 0.0) * MAGNITUDE);
    EmitVertex();
    EndPrimitive();
}

void main()
{
    GenerateLine(0);
    GenerateLine(1);
    GenerateLine(2);
}
