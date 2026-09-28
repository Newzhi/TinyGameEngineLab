#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D texture1;
uniform samplerCube skybox;
uniform vec3 cameraPos;
// 0 = 普通 2D 贴图；1 = 反射环境；2 = 折射（玻璃）
uniform int uMode;

void main()
{
    if (uMode == 1)
    {
        // 入射：相机 → 片元；reflect 得到弹开方向，用来采天空盒
        vec3 I = normalize(FragPos - cameraPos);
        vec3 R = reflect(I, normalize(Normal));
        FragColor = vec4(texture(skybox, R).rgb, 1.0);
        return;
    }

    if (uMode == 2)
    {
        // 空气(1.00) → 玻璃(1.52)；refract 得到弯折后的方向
        float ratio = 1.00 / 1.52;
        vec3 I = normalize(FragPos - cameraPos);
        vec3 R = refract(I, normalize(Normal), ratio);
        FragColor = vec4(texture(skybox, R).rgb, 1.0);
        return;
    }

    FragColor = texture(texture1, TexCoords);
}
