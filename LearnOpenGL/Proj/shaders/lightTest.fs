#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// diffuse / specular 用贴图逐片元采样；ambient 复用 diffuse 采样色
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float     shininess;
};

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;

void main()
{
    vec3 N = normalize(Normal);
    vec3 L = normalize(light.position - FragPos);
    vec3 V = normalize(viewPos - FragPos);

    vec3 texDiff = texture(material.diffuse, TexCoords).rgb;
    vec3 texSpec = texture(material.specular, TexCoords).rgb;

    // Ambient：环境光色 × 漫反射贴图
    vec3 ambient = light.ambient * texDiff;

    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = light.diffuse * diff * texDiff;

    // Specular：高光强度由镜面贴图调制（木板黑、钢框白）
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), material.shininess);
    vec3 specular = light.specular * spec * texSpec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
