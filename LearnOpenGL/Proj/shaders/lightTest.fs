#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;   // 相机世界坐标，用于镜面高光

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos); // 片元 → 光源

    // ---------- Ambient：微弱底光 ----------
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // ---------- Diffuse：兰伯特漫反射 ----------
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // ---------- Specular：Phong 镜面高光 ----------
    // V：片元 → 相机；R：光关于法线的反射方向
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm); // GLSL 要求入射指向表面，故用 -lightDir
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0); // shininess = 32
    vec3 specular = specularStrength * spec * lightColor;

    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
