#version 330 core
// 用方向采深度立方体；比较的是线性光距。环境光不乘阴影。
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

uniform sampler2D diffuseTexture;
uniform samplerCube depthMap;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float far_plane;
// 0 无阴影 / 1 硬阴影 / 2 PCF / 3 灰度深度
uniform int uShadowMode;

vec3 sampleOffsetDirections[20] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);

    if (uShadowMode == 2)
    {
        float shadow = 0.0;
        float bias = 0.15;
        int samples = 20;
        float viewDistance = length(viewPos - fragPos);
        float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;
        for (int i = 0; i < samples; ++i)
        {
            float closestDepth = texture(depthMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
            closestDepth *= far_plane;
            if (currentDepth - bias > closestDepth)
                shadow += 1.0;
        }
        return shadow / float(samples);
    }

    float closestDepth = texture(depthMap, fragToLight).r * far_plane;
    float bias = 0.05;
    return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
}

void main()
{
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;

    if (uShadowMode == 3)
    {
        float closestDepth = texture(depthMap, fs_in.FragPos - lightPos).r;
        FragColor = vec4(vec3(closestDepth), 1.0);
        return;
    }

    vec3 normal = normalize(fs_in.Normal);
    vec3 lightColor = vec3(0.3);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);

    vec3 ambient = 0.3 * color;
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;

    float shadow = 0.0;
    if (uShadowMode != 0)
        shadow = ShadowCalculation(fs_in.FragPos);

    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;
    FragColor = vec4(lighting, 1.0);
}
