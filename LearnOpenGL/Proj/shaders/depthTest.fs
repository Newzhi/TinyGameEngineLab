#version 330 core

out vec4 FragColor;

// 必须与 CPU 创建投影矩阵时使用的 near / far 相同。
uniform float nearPlane;
uniform float farPlane;

// 仅用于显示亮度，不改变真正的深度测试。
// 若直接除以 farPlane=100，距离 5 的物体只有 0.05，画面会几乎全黑。
// 所以用较小的可视化范围把近处层次拉亮；超过该距离会被 clamp 为白色。
uniform float visualizationRange;

float LinearizeDepth(float depth)
{
    // gl_FragCoord.z 是透视投影后的非线性深度，范围 [0, 1]。
    // 第一步把它还原到 NDC 的 [-1, 1]。
    float z = depth * 2.0 - 1.0;

    // 反推透视投影公式，得到观察空间中的线性距离（nearPlane ~ farPlane）。
    return (2.0 * nearPlane * farPlane)
         / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main()
{
    float linearDepth = LinearizeDepth(gl_FragCoord.z);
    float gray = clamp(linearDepth / visualizationRange, 0.0, 1.0);

    // 近处为黑、远处逐渐变白。
    // 这里只改变显示颜色；实际遮挡仍由开启的 GL_DEPTH_TEST 决定。
    FragColor = vec4(vec3(gray), 1.0);
}
