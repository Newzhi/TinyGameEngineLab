#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float     shininess;
};

// 三种投光物共用一套字段；按 lightType 选用
struct Light {
    vec3  position;
    vec3  direction;   // 平行光 / 聚光：从光源出发的方向
    float cutOff;      // 聚光内切光角余弦
    float outerCutOff; // 聚光外切光角余弦

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;
uniform int lightType; // 0 平行光 | 1 点光 | 2 聚光

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    vec3 texDiff = texture(material.diffuse, TexCoords).rgb;
    vec3 texSpec = texture(material.specular, TexCoords).rgb;

    // ---------- 按类型得到 L、衰减、聚光强度 ----------
    vec3  L;
    float attenuation = 1.0;
    float intensity   = 1.0;

    if (lightType == 0)
    {
        // 平行光：全场同一方向（习惯定义从光源射出，取反得片元→光）
        L = normalize(-light.direction);
    }
    else
    {
        // 点光 / 聚光：从片元指向光源
        vec3 toLight = light.position - FragPos;
        float distance = length(toLight);
        L = normalize(toLight);

        attenuation = 1.0 / (light.constant + light.linear * distance
                             + light.quadratic * (distance * distance));

        if (lightType == 2)
        {
            // 聚光软边：内外圆锥之间插值强度
            float theta   = dot(L, normalize(-light.direction));
            float epsilon = light.cutOff - light.outerCutOff;
            intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
        }
    }

    // ---------- Phong ----------
    vec3 ambient = light.ambient * texDiff;

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = light.diffuse * diff * texDiff;

    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), material.shininess);
    vec3 specular = light.specular * spec * texSpec;

    // 平行光不衰减；点光三项都衰减；聚光 ambient 不乘 intensity，避免锥外全黑
    ambient  *= attenuation;
    diffuse  *= attenuation * intensity;
    specular *= attenuation * intensity;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
