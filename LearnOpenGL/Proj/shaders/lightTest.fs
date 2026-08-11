#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

// 物体表面属性：对三种光的反射方式不同
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

// 灯光属性：环境 / 漫反射 / 镜面分量可分别设置
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

    // Ambient
    vec3 ambient = light.ambient * material.ambient;

    // Diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);

    // Specular (Phong)
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
