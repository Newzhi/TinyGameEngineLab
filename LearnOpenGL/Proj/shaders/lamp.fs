#version 330 core
out vec4 FragColor;

// 由 C++ 传入，使灯立方体显示实际的光源颜色
uniform vec3 lightColor;

void main()
{
    FragColor = vec4(lightColor, 1.0);
}
