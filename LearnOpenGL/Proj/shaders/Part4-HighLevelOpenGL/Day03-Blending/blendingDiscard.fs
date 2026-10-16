#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture1;

void main()
{
    vec4 texColor = texture(texture1, TexCoords);
    // 草等「非透即不透」贴图：低于阈值直接丢掉，不写颜色也不写深度
    if (texColor.a < 0.1)
        discard;
    FragColor = texColor;
}
