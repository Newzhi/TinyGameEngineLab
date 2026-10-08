#version 330 core
// 每个图元每个像素只跑一次；MSAA 按子样本覆盖率把这次颜色写入部分子样本。
in vec3 Color;
out vec4 FragColor;

void main()
{
    FragColor = vec4(Color, 1.0);
}
