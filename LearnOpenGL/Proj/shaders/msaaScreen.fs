#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

// 只能采样已经还原(resolve)过的普通 2D 纹理，不能直接采样 GL_TEXTURE_2D_MULTISAMPLE
uniform sampler2D screenTexture;
uniform int uGray;

void main()
{
    vec3 color = texture(screenTexture, TexCoords).rgb;
    if (uGray == 1)
    {
        float g = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = vec3(g);
    }
    FragColor = vec4(color, 1.0);
}
