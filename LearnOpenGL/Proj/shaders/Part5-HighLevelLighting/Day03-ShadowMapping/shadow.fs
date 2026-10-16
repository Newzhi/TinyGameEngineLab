#version 330 core
// Blinn-Phong；shadow 为 1 表示在影里。环境光不乘阴影，否则影里全黑。
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} fs_in;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;
uniform vec3 lightPos;
uniform vec3 viewPos;
// 0 无阴影 / 1 硬阴影 / 2 3×3 PCF
uniform int uShadowMode;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // 超出灯的远平面：强制不在影里（CLAMP_TO_BORDER 管不到 z）
    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    if (uShadowMode == 2)
    {
        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
            }
        }
        return shadow / 9.0;
    }

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
}

void main()
{
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightColor = vec3(1.0);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);

    vec3 ambient = 0.15 * lightColor;

    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;

    float shadow = 0.0;
    if (uShadowMode != 0)
        shadow = ShadowCalculation(fs_in.FragPosLightSpace, normal, lightDir);

    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;
    FragColor = vec4(lighting, 1.0);
}
