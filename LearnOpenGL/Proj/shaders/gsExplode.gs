#version 330 core

// 输入必须对上 glDrawArrays(GL_TRIANGLES, ...)：一次吃进 3 个顶点
layout (triangles) in;
// 输出仍是一个三角形。max_vertices = 3，三个顶点各 Emit 一次
layout (triangle_strip, max_vertices = 3) out;

in VS_OUT {
    vec3 normal;
} gs_in[];

out vec3 FragPos;
out vec3 Normal;

uniform mat4 view;
uniform mat4 projection;
uniform float time;

// 用三角形三条边叉乘出「面法线」。
// 立方体同一面上的两个三角形共面，叉乘结果平行，
// 所以两个三角形沿同一方向飞出去——看起来是整面在分离。
// 叉乘顺序会决定朝里还是朝外：和顶点法线点乘，保证往外推。
vec3 GetNormal()
{
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 n = normalize(cross(a, b));
    if (dot(n, gs_in[0].normal) < 0.0)
        n = -n;
    return n;
}

// sin(time) 在 [-1,1]，映射到 [0,1]：只往外推，再收回来，不会穿进立方体里面。
vec4 explode(vec4 position, vec3 normal)
{
    float magnitude = 0.85;
    vec3 direction = normal * ((sin(time) + 1.0) / 2.0) * magnitude;
    return position + vec4(direction, 0.0);
}

void main()
{
    vec3 faceNormal = GetNormal();

    for (int i = 0; i < 3; ++i)
    {
        vec4 world = explode(gl_in[i].gl_Position, faceNormal);
        FragPos = world.xyz;
        Normal = gs_in[i].normal; // 光照仍用顶点法线，分离后面朝向不变
        gl_Position = projection * view * world;
        EmitVertex();
    }
    EndPrimitive();
}
