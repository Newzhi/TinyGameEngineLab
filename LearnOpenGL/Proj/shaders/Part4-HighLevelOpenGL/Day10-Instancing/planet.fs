#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// 与 Mesh::Draw 的采样器名一致：material.texture_diffuse1
struct Material {
    sampler2D texture_diffuse1;
};
uniform Material material;

void main()
{
    FragColor = texture(material.texture_diffuse1, TexCoords);
}
