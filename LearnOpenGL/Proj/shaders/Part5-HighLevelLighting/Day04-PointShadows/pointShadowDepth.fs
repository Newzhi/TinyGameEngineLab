#version 330 core
// 写入灯到片元的线性距离（除以 far_plane 落到 [0,1]），采样时用同样的 length 去比。
in vec4 FragPos;

uniform vec3 lightPos;
uniform float far_plane;

void main()
{
    float lightDistance = length(FragPos.xyz - lightPos);
    gl_FragDepth = lightDistance / far_plane;
}
