#version 330 core
// 法线图在切线空间；L / V 已在顶点里变过来。光照公式与 Day01 相同。
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} fs_in;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
// 0 几何法线 / 1 法线贴图 / 2 显示解包后的法线
uniform int uMode;

void main()
{
    vec3 normal = vec3(0.0, 0.0, 1.0);
    if (uMode != 0)
    {
        normal = texture(normalMap, fs_in.TexCoords).rgb;
        normal = normalize(normal * 2.0 - 1.0);
    }

    if (uMode == 2)
    {
        FragColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }

    vec3 color = texture(diffuseMap, fs_in.TexCoords).rgb;
    vec3 lightDir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
    vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);

    vec3 ambient = 0.1 * color;
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * color;

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = vec3(0.2) * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
