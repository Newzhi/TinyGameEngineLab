#version 330 core
// Phong：spec = (V·R)^shininess，夹角 > 90° 时整项为 0，低反光度会出现硬边。
// Blinn-Phong：H = normalize(L+V)，spec = (N·H)^shininess；灯在表面上方时夹角不超过 90°。
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

uniform sampler2D floorTexture;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform bool blinn;
uniform float shininess;

void main()
{
    vec3 color = texture(floorTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);

    vec3 ambient = 0.05 * color;

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * color;

    float spec = 0.0;
    if (blinn)
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    }
    else
    {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    }
    // 高光不乘漫反射贴图，灰色光斑在木纹上更清楚。
    vec3 specular = lightColor * spec * 0.3;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
