# 投光物（Light Casters）（Day 05）

对应 LearnOpenGL Part 2「投光物」章节：**把「空间中一个点、永不衰减」升级为多种光源类型**——平行光、带衰减的点光源、聚光（手电筒）。

| 文档 | 内容 |
|------|------|
| [Day04 光照贴图](../Day04/光照贴图LightingMaps.md) | Diffuse / Specular 贴图、Phong 公式 |
| **本文档** | Directional / Point / Spotlight、衰减、切光角与软边 |
| 后续 | 多光源（把多种投光物合进同一场景） |

> **公式说明**：公式用代码块书写，便于 Markdown 预览。

**前置**：已掌握 Phong（ambient/diffuse/specular）+ 光照贴图；相机 `Position` / `Front`（聚光手电筒会用到）。

---

## 1. 什么是投光物？

此前 Demo 里的灯：世界空间有一个 **位置**，朝四周发光，且 **不随距离变暗**——这是简化点光。

现实中光源种类很多。把光「投射」到物体上的东西叫 **投光物（Light Caster）**。本章三种：

| 类型 | 直觉 | 关键输入 | 典型例子 |
|------|------|----------|----------|
| **平行光 / 定向光** Directional | 所有光线方向相同 | `direction`（与位置无关） | 太阳 |
| **点光源** Point | 一点朝四面八方，随距离衰减 | `position` + 衰减系数 | 灯泡、火把 |
| **聚光** Spotlight | 一点朝特定方向，只照圆锥内 | `position` + `direction` + 切光角 | 路灯、手电筒 |

下一章再讨论如何在**同一场景**里同时用多种类型。

---

## 2. 平行光（Directional Light）

### 2.1 思路

光源极远时，到达场景的光线可近似 **互相平行**，对每个物体方向都一样——与「灯在哪」无关，只关心 **光从哪个方向来**。

太阳虽非无限远，但对场景尺度可当作定向光。

### 2.2 Light 结构变化

```glsl
struct Light {
    // vec3 position;  // 不再需要
    vec3 direction;    // 从光源出发的全局方向（习惯定义）

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
```

计算 `lightDir`（片元 → 光源方向，供 Phong 使用）：

```
lightDir = normalize(-light.direction)
```

原因：习惯上 `direction` 表示「光往哪射」（如朝下 `(−0.2, −1.0, −0.3)`），而 Phong 需要的是「从片元指向光源」→ 取反后再 normalize。

漫反射 / 镜面公式其余不变，只是 `L` 对全场片元相同。

### 2.3 C++ 示例

```cpp
lightingShader.setVec3("light.direction", -0.2f, -1.0f, -0.3f);
```

为展示「全场方向一致」，常用多箱子场景（各自 `translate` + `rotate` 后 `draw`），移动相机时明暗方向不变，像天空有一轮太阳。

### 2.4 扩展：`vec4` 的 w 分量技巧（了解即可）

旧固定管线习惯：

| `vec4` 的 w | 含义 |
|-------------|------|
| `1.0` | 位置向量（会受平移影响） |
| `0.0` | 方向向量（不受平移） |

可据此分支做定向光 vs 位置光；现代 shader 里更常见的是直接用不同字段 / 不同 uniform。

---

## 3. 点光源（Point Light）与衰减

### 3.1 思路

点光 = 世界中某点 + **朝所有方向**发光。此前我们缺的是 **衰减（Attenuation）**：远处应变暗，否则后排箱子和灯前一样亮。

线性衰减往往假；现实近处很亮、先快后慢减弱。常用二次公式：

```
Fatt = 1.0 / (Kc + Kl * d + Kq * d²)

d  = 片元到光源的距离
Kc = 常数项（常为 1.0，防止分母 < 1 反而变亮）
Kl = 一次项（随距离线性变暗）
Kq = 二次项（远距离主导，下降更快）
```

近处二次项影响小；距离变大后二次项超过一次项 → 先较快变暗，再缓慢趋近 0。

### 3.2 衰减系数参考表（Ogre3D Wiki）

按「希望覆盖的大致半径」选一组起步值；常数项一律 1.0。

| 距离 | 常数 Kc | 一次 Kl | 二次 Kq |
|------|---------|---------|---------|
| 7 | 1.0 | 0.7 | 1.8 |
| 13 | 1.0 | 0.35 | 0.44 |
| 20 | 1.0 | 0.22 | 0.20 |
| 32 | 1.0 | 0.14 | 0.07 |
| 50 | 1.0 | 0.09 | 0.032 |
| 65 | 1.0 | 0.07 | 0.017 |
| 100 | 1.0 | 0.045 | 0.0075 |
| 160 | 1.0 | 0.027 | 0.0028 |
| 200 | 1.0 | 0.022 | 0.0019 |
| 325 | 1.0 | 0.014 | 0.0007 |
| 600 | 1.0 | 0.007 | 0.0002 |
| 3250 | 1.0 | 0.0014 | 0.000007 |

教程示例常用覆盖约 **50** 的一组：`1.0 / 0.09 / 0.032`。多数场景 **32～100** 足够起步，再凭观感微调。

### 3.3 Light 结构与实现

```glsl
struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};
```

```cpp
lightingShader.setFloat("light.constant",  1.0f);
lightingShader.setFloat("light.linear",    0.09f);
lightingShader.setFloat("light.quadratic", 0.032f);
```

片元中：

```
d           = length(light.position - FragPos)
attenuation = 1.0 / (constant + linear * d + quadratic * d * d)

ambient  *= attenuation
diffuse  *= attenuation
specular *= attenuation
```

`lightDir` 仍用 `normalize(light.position - FragPos)`（与定向光不同）。

> **环境光是否衰减**：单灯时可让 ambient 不衰减，避免全黑；多灯时 ambient 常也乘衰减，否则多盏灯的环境光会叠得过亮。以场景观感为准。

效果：近箱亮、远箱暗甚至几乎不亮——这才是「局部点光」。

---

## 4. 聚光（Spotlight）

### 4.1 思路

位置 + **朝一个方向**照射；只有落在圆锥内的片元被照亮。路灯、手电筒都是聚光。

关键量（对每个片元）：

| 符号 / 名 | 含义 |
|-----------|------|
| `LightDir` | 片元指向光源（或与计算约定一致的光方向） |
| `SpotDir` | 聚光朝向 |
| `θ` (theta) | `LightDir` 与 `SpotDir` 夹角 |
| `ϕ` (phi) | 切光角（圆锥半角半径） |

片元在锥内 ⟺ `θ < ϕ`（角度意义）。实现上用 **余弦** 比较，避免 `acos`。

### 4.2 手电筒（Flashlight）

手电筒 = 聚光，但位置/方向跟相机走：

```cpp
lightingShader.setVec3("light.position",  camera.Position);
lightingShader.setVec3("light.direction", camera.Front);
lightingShader.setFloat("light.cutOff",   glm::cos(glm::radians(12.5f)));
```

注意：传入的是 **切光角的余弦**，不是角度本身——因为 FS 里 `dot` 得到的也是余弦。

```glsl
struct Light {
    vec3  position;
    vec3  direction;
    float cutOff;   // cos(ϕ)
    // ambient / diffuse / specular / 衰减等可按需保留
};
```

硬边版本：

```
lightDir = normalize(light.position - FragPos)   // 按项目约定
theta    = dot(lightDir, normalize(-light.direction))

if (theta > light.cutOff) {
    // 做完整 Phong（可再乘点光衰减）
} else {
    // 仅 ambient，避免锥外全黑
}
```

为何是 `theta > cutOff`？  
余弦：0° → 1.0，90° → 0.0。角越小余弦越大。锥内角小 → `theta` 更接近 1 → 应 **大于** `cos(ϕ)`。

硬边问题：碰到圆锥边界突然变黑，假。

### 4.3 平滑 / 软边（内外圆锥）

引入内切光 `ϕ`（`cutOff`）与外切光 `γ`（`outerCutOff`），边缘强度从 1 过渡到 0：

```
ε = ϕ − γ          // 都用余弦值：ε = cutOff - outerCutOff
I = (θ − γ) / ε
I = clamp(I, 0.0, 1.0)
```

| 区域 | 强度 I |
|------|--------|
| 内圆锥内 | ≈ 1 |
| 内外之间 | 0～1 插值 |
| 外圆锥外 | 0 |

```
intensity = clamp((theta - outerCutOff) / (cutOff - outerCutOff), 0.0, 1.0)

diffuse  *= intensity
specular *= intensity
// ambient 常不乘，锥外仍有一点底光
```

教程常用：内角 12.5°、外角 17.5°（均转为 `cos` 再上传）。

---

## 5. 三种投光物对照

| | 平行光 | 点光源 | 聚光 |
|---|--------|--------|------|
| 位置 | 不需要 | 需要 | 需要 |
| 方向 | 全局 `direction` | 无（四面八方） | `direction` + 切光 |
| `lightDir` | `normalize(-direction)` | `normalize(pos - FragPos)` | 同点光 + 锥内判断 |
| 衰减 | 通常无（太阳） | `1/(Kc+Kl·d+Kq·d²)` | 常叠加衰减 |
| 额外 | — | Kc/Kl/Kq | cutOff / outerCutOff |
| Demo 观感 | 全场同向明暗 | 近亮远暗 | 只照前方圆锥（可跟相机） |

Phong 三项公式不变；变的是 **L 怎么来**、以及是否再乘 **attenuation / intensity**。

---

## 6. 片元侧伪代码串起来

```
// --- 取材质贴图（Day04）---
texDiff = texture(diffuseMap, uv).rgb
texSpec = texture(specularMap, uv).rgb

// --- 按光源类型得到 L，以及可选 attenuation / intensity ---
若 平行光:
  L = normalize(-light.direction)
  att = 1, intensity = 1

若 点光源:
  L = normalize(light.position - FragPos)
  d = length(light.position - FragPos)
  att = 1 / (Kc + Kl*d + Kq*d*d)
  intensity = 1

若 聚光:
  L = normalize(light.position - FragPos)
  theta = dot(L, normalize(-light.direction))
  intensity = clamp((theta - outerCutOff) / (cutOff - outerCutOff), 0, 1)
  （可选再乘点光衰减 att）

// --- Phong ---
ambient  = light.ambient  * texDiff
diffuse  = light.diffuse  * max(N·L, 0) * texDiff
specular = light.specular * pow(max(V·R, 0), shininess) * texSpec

ambient  *= att                 // 是否乘视多灯策略
diffuse  *= att * intensity
specular *= att * intensity

FragColor = ambient + diffuse + specular
```

---

## 7. 和当前项目的关系

当前工程已实现 **投光物 Demo**（`main.cpp` + `lightTest.fs`）：

| 操作 | 效果 |
|------|------|
| 启动 | 随机生成 10 个箱子（固定种子 42，可复现） |
| 按键 `1` | **平行光**：全场同向，像太阳 |
| 按键 `2` | **点光 + 衰减**：小灯立方体绕场旋转，近亮远暗 |
| 按键 `3` | **聚光 / 手电筒**：跟相机位置与 `Front`，软边圆锥 |

Shader 用 `uniform int lightType` 分支；Phong + 光照贴图保留。WASD + 鼠标漫游。

---

## 8. 小练习

1. 平行光：改 `direction`，确认所有箱子明暗方向一起变。  
2. 点光：用表中「距离 32」与「距离 100」两组系数对比覆盖范围。  
3. 点光：只对 diffuse/specular 衰减、ambient 不衰减，对比全黑风险。  
4. 聚光：硬边 `if (theta > cutOff)` vs 软边 `intensity`。  
5. 手电筒：内外角 12.5/17.5 改为更大/更小，感受锥宽与软边宽度。  
6. （思考）为何切光角要传 `cos`，而不是在 shader 里对 `dot` 结果做 `acos`？

---

## 9. 公式速查

```
// 平行光
L = normalize(-light.direction)

// 点光衰减
d   = length(light.position - FragPos)
Fatt = 1 / (Kc + Kl*d + Kq*d²)

// 聚光软边（余弦域）
θ = dot(L, normalize(-spotDir))
I = clamp((θ - cosγ) / (cosϕ - cosγ), 0, 1)

FragColor ≈ (ambient + diffuse + specular) 经 att、I 调制
```

---

## 10. 阅读顺序与下一步

```
Day04 光照贴图
  ↓
Day05 投光物（本文）← 平行光 / 点光衰减 / 聚光
  ↓
多光源（多种投光物同时作用）
```

---

## 11. 参考文献

1. LearnOpenGL CN — [投光物](https://learnopengl-cn.github.io/02%20Lighting/05%20Light%20casters/)
2. LearnOpenGL EN — [Light casters](https://learnopengl.com/Lighting/Light-casters)
3. Ogre3D Wiki — 点光衰减系数参考表（本文第 3.2 节）
4. 《Part2 Day04 — 光照贴图》— 贴图采样与 Phong 合成
5. 《Part1 Day07 — 摄像机》— `Position` / `Front`（手电筒）
