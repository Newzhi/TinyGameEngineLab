#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 viewPos;

void main()
{
    vec3 N = normalize(Normal);
    vec3 L = normalize(-lightDir);
    float diff = max(dot(N, L), 0.15);
    vec3 color = vec3(0.35, 0.62, 0.95) * diff;
    FragColor = vec4(color, 1.0);
}
