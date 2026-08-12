# 多光源（Multiple Lights）（Day 06）

对应 LearnOpenGL Part 2「[多光源](https://learnopengl-cn.github.io/02%20Lighting/06%20Multiple%20lights/)」：**把 Day05 的平行光、点光、聚光同时放进一个场景**，每个光源各自算一遍贡献，再 **相加** 得到最终颜色。

| 文档 | 内容 |
|------|------|
| [Day05 投光物](../Day05/投光物LightCasters.md) | 三种灯各自怎么算（一次只用一种） |
| **本文档** | 多盏灯叠加、GLSL 函数封装、数组 uniform、术语中英对照 |
| 后续 | [Part3 模型加载 — Assimp](../../Part3-LoadModel/Day01/Assimp模型导入库.md) |

> **公式说明**：公式用代码块书写，便于 Markdown 预览。

**前置**：已理解 Day05 的 `L`、衰减系数、聚光强度系数，以及 Phong 三项。

---

## 0. 术语中英对照（先看这里）

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Multiple lights | **多光源** | 场景里同时有很多盏灯 |
| Contribution | **贡献** | 某一盏灯对这个像素「加了多少颜色」 |
| Directional light / DirLight | **平行光 / 定向光** | 像太阳，全场同方向 |
| Point light / PointLight | **点光源** | 像灯泡，有位置、会衰减 |
| Spotlight / SpotLight | **聚光** | 像手电筒，只照圆锥 |
| Ambient / Diffuse / Specular | **环境光 / 漫反射 / 镜面高光** | Phong 三项 |
| Attenuation | **衰减系数** | 距离越远越小的乘数 |
| Intensity（聚光里） | **聚光强度系数** | 在不在光束里的乘数 |
| Array uniform | **数组形式的 uniform** | 如 `pointLights[4]` |

---

## 1. Day05 → Day06：差在哪里？

| | Day05（当前工程） | Day06（本章） |
|---|-------------------|----------------|
| 灯的数量 | **一次只用一种**（按键 1/2/3 切换） | **同时**有太阳 + 多个点光 + 可选手电筒 |
| Shader 结构 | `if (lightType == …)` 分支 | 每种灯一个 **函数**，`main` 里 **累加** |
| 最终颜色 | 一盏灯算出来的结果 | `结果 = 灯A贡献 + 灯B贡献 + …` |

核心思想（官网结构）：

```
最终颜色 = 0
最终颜色 += 平行光的贡献
最终颜色 += 点光0的贡献 + 点光1的贡献 + …
最终颜色 += 聚光的贡献（可选）
```

两盏灯都照到同一像素 → 两边贡献 **加在一起** → 比单灯更亮。这不是相乘，是 **相加叠加**。

---

## 2. 为什么要用 GLSL 函数？

每种灯算法不同。若全写在 `main` 里，代码又长又难改。

做法：为每种灯写一个函数，输入灯参数 + 法线 + 视线等，**返回这一盏灯贡献的颜色向量**。

```
CalcDirLight(...)   → 返回平行光贡献颜色
CalcPointLight(...) → 返回某一个点光贡献颜色
CalcSpotLight(...)  → 返回聚光贡献颜色
```

和 C 一样：若函数定义在 `main` **下面**，要在上面先写 **原型声明**。

---

## 3. 平行光函数（定向光）

### 3.1 结构体

```glsl
struct DirLight {
    vec3 direction;   // 光往哪射

    vec3 ambient;     // 环境光分量
    vec3 diffuse;     // 漫反射分量
    vec3 specular;    // 镜面分量
};
uniform DirLight dirLight;
```

### 3.2 原型与实现要点

```glsl
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
```

内部就是 Day05 平行光逻辑，整理成函数后 **返回** `环境光+漫反射+高光`：

```
光方向 L = normalize(-light.direction)
漫反射系数 = max(法线·L, 0)
高光系数   = pow(max(视线·反射方向, 0), 反光度)

环境光 = light.ambient  × 漫反射贴图色
漫反射 = light.diffuse  × 漫反射系数 × 漫反射贴图色
高光   = light.specular × 高光系数 × 镜面贴图色

返回 环境光 + 漫反射 + 高光
```

无距离衰减（太阳近似）。

---

## 4. 点光源函数（可多盏）

### 4.1 结构体 + 数组

```glsl
struct PointLight {
    vec3 position;    // 灯的位置

    float constant;   // 衰减常数项
    float linear;     // 衰减一次项
    float quadratic;  // 衰减二次项

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NR_POINT_LIGHTS 4
uniform PointLight pointLights[NR_POINT_LIGHTS];
```

`#define NR_POINT_LIGHTS 4`：预处理常量，表示点光数量。  
`pointLights[4]`：四个点光的参数数组。

### 4.2 函数要点

```glsl
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
```

比平行光多：**距离** 与 **衰减系数**：

```
L = normalize(灯位置 - 片元位置)
距离 d = length(灯位置 - 片元位置)
衰减系数 = 1 / (常数项 + 一次项×d + 二次项×d²)

先算环境光、漫反射、高光（同 Phong）
再：三项都 × 衰减系数
返回三项之和
```

`main` 里循环：

```glsl
for (int i = 0; i < NR_POINT_LIGHTS; i++)
    result += CalcPointLight(pointLights[i], norm, FragPos, viewDir);
```

同一套函数算 4 盏灯，不必复制四遍代码。

---

## 5. 聚光函数（练习 / 可选）

官网把 `CalcSpotLight` 留给练习。思路与 Day05 手电筒相同：在点光基础上再乘 **聚光强度系数**（内外圆锥软边）。

结构体通常还要：

```
position、direction、cutOff（内切光余弦）、outerCutOff（外切光余弦）
以及 ambient/diffuse/specular、可选衰减三项
```

`main` 里：

```
result += CalcSpotLight(spotLight, norm, FragPos, viewDir);
```

手电筒时：位置=相机位置，方向=相机朝前。

---

## 6. 合并：`main` 里累加

```glsl
void main()
{
    vec3 norm    = normalize(Normal);                 // 法线
    vec3 viewDir = normalize(viewPos - FragPos);      // 视线方向

    // 第一阶段：平行光（太阳）
    vec3 result = CalcDirLight(dirLight, norm, viewDir);

    // 第二阶段：多个点光
    for (int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir);

    // 第三阶段：聚光（可选）
    // result += CalcSpotLight(spotLight, norm, FragPos, viewDir);

    FragColor = vec4(result, 1.0);
}
```

| 阶段 | 做什么 |
|------|--------|
| 1 | 加上太阳式全局照明 |
| 2 | 循环加上每个灯泡的局部照明 |
| 3 | 加上手电筒圆锥 |

注意：每盏灯函数内部若都加一份环境光，多灯时环境光会 **叠得很亮**——调参时常把各灯的 `ambient` 设得很小。

---

## 7. C++：怎么设数组里的 uniform？

结构体数组要带 **下标**：

```cpp
lightingShader.setFloat("pointLights[0].constant", 1.0f);
lightingShader.setVec3("pointLights[0].position", x, y, z);
// pointLights[1].…、[2].…、[3].… 同理
```

教程示例四个点光位置：

```cpp
glm::vec3 pointLightPositions[] = {
    glm::vec3( 0.7f,  0.2f,  2.0f),
    glm::vec3( 2.3f, -3.3f, -4.0f),
    glm::vec3(-4.0f,  2.0f, -12.0f),
    glm::vec3( 0.0f,  0.0f, -3.0f)
};
```

绘制：对每个点光画一个小灯立方体（各自 `model` 平移到对应位置），和画多个箱子类似。

平行光：

```cpp
lightingShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
lightingShader.setVec3("dirLight.ambient", ...);
// …
```

四个点光 × 多个成员 → uniform 调用很多，可封装成「点光类 / 辅助函数」减少重复，但底层仍是这些名字。

---

## 8. 叠加直觉（数字）

某像素上：

| 来源 | 贡献（示意） |
|------|----------------|
| 平行光 | (0.2, 0.2, 0.2) |
| 点光 0（很近） | (0.5, 0.4, 0.3) |
| 点光 1（很远） | (0.02, 0.02, 0.02) |
| 手电筒（正对） | (0.3, 0.3, 0.3) |

```
最终 ≈ 以上相加 → 比任何单灯都亮
```

若某灯贡献接近 0（太远或在锥外），加了也几乎看不见——这就是「有的区域只有太阳底光、有的区域还有灯泡」。

---

## 9. 和当前项目的关系

当前工程已实现 **多光源 Demo**（`main.cpp` + `lightTest.fs`）：

| 内容 | 说明 |
|------|------|
| 平行光 | `dirLight`，像太阳的全局底光 |
| 4 个点光 | `pointLights[0..3]`，不同颜色；场景里画 4 个对应颜色的小立方体 |
| 手电筒 | `spotLight` 跟相机；按 `F` 开关 |
| Shader | `CalcDirLight` / `CalcPointLight` / `CalcSpotLight`，`result +=` 累加 |
| 箱子 | 仍为随机布局的 10 个木箱 + 光照贴图 |

观察：走近彩色小灯时局部偏色；手电筒开时前方圆锥更亮；关手电筒仍有太阳 + 点光。

---

## 10. 调参与氛围（官网提示）

关卡/美术常调的是各灯的 `ambient/diffuse/specular` 和衰减。  
同一套几何，只改这些数，就能从「明亮室外」变成「阴暗室内 + 手电筒」。

练习（对齐官网）：

1. 实现 `CalcSpotLight` 并加进 `result`。  
2. 把四个点光设成不同颜色，观察叠加色。  
3. 把各灯 `ambient` 调大，看「发灰发亮」的过曝。  
4. （官网）调节光照属性，尝试复现教程末尾不同氛围图。

---

## 11. 公式 / 流程速查

```
对每个片元：
  N = 归一化法线
  V = 归一化(相机位置 - 片元位置)

  result  = CalcDirLight(平行光, N, V)
  result += Σ CalcPointLight(点光[i], N, 片元位置, V)
  result += CalcSpotLight(聚光, N, 片元位置, V)   // 可选

  输出颜色 = result
```

每盏灯内部仍是：

```
贡献 = (环境光 + 漫反射 + 高光) × 衰减系数 × [聚光强度系数]
```

---

## 12. 阅读顺序与下一步

```
Day05 投光物（一次一种灯）
  ↓
Day06 多光源（本文）← 函数封装 + 贡献相加
  ↓
Part3 Day01 Assimp ← 模型导入库
  ↓
Mesh / Model 类
```

---

## 13. 参考文献

1. LearnOpenGL CN — [多光源](https://learnopengl-cn.github.io/02%20Lighting/06%20Multiple%20lights/)
2. LearnOpenGL EN — [Multiple lights](https://learnopengl.com/Lighting/Multiple-lights)
3. 《Part2 Day05 — 投光物》— 单灯公式与衰减 / 聚光系数
4. 《Part2 Day04 — 光照贴图》— 漫反射 / 镜面贴图采样
