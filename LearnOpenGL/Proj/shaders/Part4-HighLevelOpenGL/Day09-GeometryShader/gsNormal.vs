#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// 位置和法线都先变到观察空间，投影留给几何着色器画线段。
out VS_OUT {
    vec3 normal;
} vs_out;

uniform mat4 model;
uniform mat4 view;

void main()
{
    gl_Position = view * model * vec4(aPos, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));
    vs_out.normal = normalize(normalMatrix * aNormal);
}
