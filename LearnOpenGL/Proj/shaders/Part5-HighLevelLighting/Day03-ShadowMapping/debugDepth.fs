#version 330 core
// 正交投影的深度图接近线性，直接把 .r 当灰度。透视深度要先线性化才能看清。
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;

void main()
{
    float depthValue = texture(depthMap, TexCoords).r;
    FragColor = vec4(vec3(depthValue), 1.0);
}
