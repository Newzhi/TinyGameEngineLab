#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// 位置先停在世界空间，投影留给几何着色器。
// 若在这里乘 projection，爆破的叉乘法线会在裁剪空间里被透视拉歪。
out VS_OUT {
    vec3 normal;
} vs_out;

uniform mat4 model;

void main()
{
    gl_Position = model * vec4(aPos, 1.0);
    vs_out.normal = normalize(mat3(transpose(inverse(model))) * aNormal);
}
