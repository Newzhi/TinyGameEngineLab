#version 330 core
// 四盏点光 Blinn-Phong。
// gamma=false：1/d 衰减，颜色直接写出（旧管线，靠显示器曲线「看着对」）。
// gamma=true ：1/d² 衰减，最后 pow(1/2.2) 抵消显示器；贴图须为 GL_SRGB。
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

uniform sampler2D floorTexture;
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform vec3 viewPos;
uniform bool gamma;

void main()
{
    vec3 color = texture(floorTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 ambient = 0.03 * color;
    vec3 lighting = vec3(0.0);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);

    for (int i = 0; i < 4; ++i)
    {
        vec3 lightDir = normalize(lightPositions[i] - fs_in.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);
        vec3 diffuse = diff * color * lightColors[i];

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
        vec3 specular = spec * lightColors[i];

        float distance = length(lightPositions[i] - fs_in.FragPos);
        // 未校正时 1/d 经过显示器 ^2.2 才有点像物理；校正后用平方反比。
        float attenuation = 1.0 / (gamma ? (distance * distance) : distance);
        lighting += (diffuse + specular) * attenuation;
    }

    color = ambient + lighting;
    if (gamma)
        color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
