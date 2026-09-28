⬆️#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
// 0=原图  1=反相  2=灰度  3=锐化核
uniform int uEffect;

void main()
{
    vec3 color = texture(screenTexture, TexCoords).rgb;

    if (uEffect == 1)
    {
        // 反相：每个通道取 1 - c
        color = vec3(1.0) - color;
    }
    else if (uEffect == 2)
    {
        // 灰度：按人眼敏感度加权
        float g = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = vec3(g);
    }
    else if (uEffect == 3)
    {
        // 锐化：对当前像素周围的 3×3 纹素做卷积。
        // textureSize 返回颜色附件的真实像素尺寸；1 / size 就是「一个纹素」
        // 在 UV [0,1] 坐标中的跨度，比官网演示用的固定 1/300 更适合窗口尺寸变化。
        vec2 texelSize = 1.0 / vec2(textureSize(screenTexture, 0));
        vec2 offsets[9] = vec2[](
            vec2(-texelSize.x,  texelSize.y), vec2(0.0,  texelSize.y), vec2(texelSize.x,  texelSize.y),
            vec2(-texelSize.x,  0.0),         vec2(0.0,  0.0),         vec2(texelSize.x,  0.0),
            vec2(-texelSize.x, -texelSize.y), vec2(0.0, -texelSize.y), vec2(texelSize.x, -texelSize.y)
        );
        // 中心权重 9，周围八个权重 -1：
        // 保留自身颜色，同时减掉邻居颜色，使明暗边界更突出。
        float kernel[9] = float[](
            -1.0, -1.0, -1.0,
            -1.0,  9.0, -1.0,
            -1.0, -1.0, -1.0
        );

        vec3 sampleTex[9];
        for (int i = 0; i < 9; ++i)
            sampleTex[i] = texture(screenTexture, TexCoords + offsets[i]).rgb;

        color = vec3(0.0);
        for (int i = 0; i < 9; ++i)
            color += sampleTex[i] * kernel[i];
    }
    // uEffect == 0：直通，不改 color

    FragColor = vec4(color, 1.0);
}
