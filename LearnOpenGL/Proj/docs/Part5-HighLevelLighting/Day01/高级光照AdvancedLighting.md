# 高级光照 Advanced Lighting（Day 01）

对应 LearnOpenGL「[高级光照](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/01%20Advanced%20Lighting/)」：Part2 的 Phong 已经能画出明暗和高光，但镜面项在**低反光度**时会出现一圈硬边。Blinn-Phong 用半程向量代替反射向量，把这条断层消掉，早期固定管线时代的 OpenGL 用的就是它。

| 文档 | 内容 |
|------|------|
| [Part2 Day02 基础光照](../../Part2-Lighting/Day02/基础光照与法线.md) | Ambient / Diffuse / Specular、`V·R` Phong 高光 |
| [Part4 Day11 抗锯齿](../../Part4-HighLevelOpenGL/Day11/抗锯齿AntiAliasing.md) | 上一阶段末章 |
| **本文档** | Phong 高光 90° 截断、半程向量、Blinn-Phong、反光度对照 |
| [Day02 Gamma 校正](../Day02/Gamma校正GammaCorrection.md) | sRGB、输出 `pow`、平方衰减 |

**工程对照**：当前 `main.cpp` 是一块带贴图的地板 + 点光。`1` Phong（默认，低 shininess 时高光边缘有断层）；`2` 或 `B` 切到 Blinn-Phong；`[` / `]` 改反光度。对着灯附近的地板高光看一圈。

**前置**：Part2 Day02 的 `N` / `L` / `V` / `R`，以及 `pow(max(dot(...), 0), shininess)`。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Phong lighting | **风氏 / Phong 光照** | Ambient + Diffuse + `V·R` 镜面 |
| Blinn-Phong | **Blinn-Phong** | 镜面改成 `N·H`，其余两项不变 |
| Halfway vector | **半程向量** | 光方向与视线方向相加再单位化 |
| Reflection vector | **反射向量** | 入射光绕法线弹开的方向 `R` |
| Shininess | **反光度 / 高光指数** | `pow` 的指数，越大光斑越尖 |
| Specular cutoff | **高光断层** | `V·R < 0` 时高光被直接掐成 0 |

---

## 1. Phong 高光哪里会断

镜面项：

```
R    = reflect(-L, N)
spec = pow(max(V · R, 0), shininess)
```

`max(..., 0)` 的意思是：视线和反射方向夹角一旦 ≥ 90°，点积为负，高光整项变成 0。

漫反射里这是对的：`N·L < 0` 表示灯在表面背面，不该有 Lambert 贡献。镜面测的却不是灯和法线，而是**眼睛和反射光线**。灯仍在表面上方时，你仍可能站在反射方向的「另一侧」，`V·R` 照样为负。

反光度很大时，高光本来就只剩针尖大，那些「反向」的贡献本来也看不见。反光度很小（教程里地板 `shininess = 1` 或本 Demo 默认 `8`）时，光斑铺得很开，90° 那条边界会变成一道硬边。

```
         V 与 R 夹角 < 90°          V 与 R 夹角 > 90°
              V                           V
             ↗                             ↖
            ●──── R                       ●──── R     ← 高光被掐成 0
           /                             /
          L                             L
```

---

## 2. 半程向量

1977 年 James F. Blinn 改了一处：不再造 `R`，改用光和视线中间那条单位向量。

```
H = normalize(L + V)
```

`H` 越贴法线 `N`，说明你越接近「正好对着反射方向看」，高光越强。视线与旧的 `R` 对齐时，`H` 正好落在 `N` 上。

只要灯还在表面上方，`N·H` 就不会超过 90°（灯在表面以下才可能）。于是低反光度时那圈断层消失，高光边缘是连续衰减。

GLSL：

```glsl
vec3 lightDir   = normalize(lightPos - FragPos);
vec3 viewDir    = normalize(viewPos - FragPos);
vec3 halfwayDir = normalize(lightDir + viewDir);
float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
vec3 specular = lightColor * spec;
```

环境光、漫反射、点光位置全部照旧。本 Demo 用 `uniform bool blinn` 在两种镜面之间切换。

---

## 3. 两种镜面差在哪

| | Phong | Blinn-Phong |
|---|--------|-------------|
| 测哪个角 | `V` 与 `R` | `N` 与 `H` |
| 夹角会不会 > 90° | 会（眼睛在反射另一侧） | 灯在表面上方时不会 |
| 低 shininess | 高光边缘可能有硬边 | 连续 |
| 同样指数下的光斑 | 更「软」、更大 | 更尖（`N·H` 通常小于 `V·R` 那个角） |

`N` 与 `H` 的夹角通常小于 `V` 与 `R` 的夹角，同样的 `shininess` 下 Blinn 看起来更锐。想接近 Phong 的观感，把 Blinn 的指数调到 Phong 的 **2～4 倍**。教程对照：Phong `8` 对 Blinn `32`。

本 Demo 两种模型**共用**同一个 `shininess`，方便直接比「同一指数下谁更尖」；用 `]` 把指数加到 32，再在 `1` / `2` 之间切，能对上教程那组图。

早期 OpenGL 固定管线用的就是 Blinn-Phong。后面 Gamma、阴影、PBR 之前，实时项目里它仍是默认选择。

---

## 4. 和本工程怎么接

### 4.1 画面与按键

大平面地板（`container.jpg` 重复贴）、头顶一盏点光（小白立方体标位置）。相机略高于地面。低反光度时绕着灯走，看脚下高光圈。

| 键 | 作用 |
|----|------|
| `1` | Phong：`spec = (V·R)^shininess` |
| `2` 或 `B` | 切换 / 切到 Blinn-Phong：`spec = (N·H)^shininess` |
| `[` / `]` | 反光度 ÷2 / ×2（1～256） |
| WASD、鼠标、滚轮、Esc | 与前面几章相同 |

### 4.2 着色器（对照 `shaders/blinnPhong.*`）

顶点着色器输出世界空间位置、法线、UV（接口块 `VS_OUT`，接 Part4 Day08）。

片元着色器：

```
color    = texture(floorTexture, uv)
ambient  = 0.05 * color
diffuse  = max(N·L, 0) * color
若 blinn:  H = normalize(L+V); spec = pow(max(N·H, 0), shininess)
否则:      R = reflect(-L, N); spec = pow(max(V·R, 0), shininess)
specular = 0.3 * spec
FragColor = ambient + diffuse + specular
```

高光不乘漫反射贴图，只乘一档灰白，地板才看得出那圈斑。灯立方体走现成的 `lamp.vs` / `lamp.fs`。

### 4.3 每一帧

设 `viewPos`、`lightPos`、`blinn`、`shininess`，画地板；再画缩放后的灯立方体。

相关文件：`src/cppfile/main.cpp`，`shaders/blinnPhong.vs` / `blinnPhong.fs`，`shaders/lamp.vs` / `lamp.fs`。

---

## 5. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 切 Blinn 也看不到高光圈 | 没传 `viewPos`，或相机贴在灯正上方、`H` 几乎等于 `L` 但你没看向反射方向 |
| Phong 看不到断层 | `shininess` 太大，光斑还没铺到 90° 边界。先按 `[` 降到 8 或 4 |
| Blinn 和 Phong「差不多」 | 站得太近、只看见光斑中心。退后、斜看地板边缘 |
| 地板全黑 | 贴图路径失败；或法线没单位化、`model` 把法线转错 |
| `H = L + V` 没 normalize | 模长不是 1，`N·H` 被放大/缩小，高光发爆或消失 |
| `reflect(lightDir, N)` | GLSL `reflect(I, N)` 的 `I` 要指向表面，应写 `reflect(-lightDir, N)` |

---

## 6. 小练习

1. 为什么漫反射可以用 `max(N·L, 0)` 把背面掐掉，镜面却不能把 `V·R < 0` 当成「没光」？
2. 写出 `H` 的公式。视线正好沿 `R` 看过来时，`H` 和 `N` 是什么关系？
3. 同一 `shininess = 8`，为什么 Blinn 的光斑通常更小更尖？
4. 本 Demo 模式 1 要看到断层，相机大概该站在灯的哪一侧、看向哪里？
5. （思考）后面若做能量守恒 / PBR，还会不会直接用 `pow(N·H, shininess)`？差在哪一层？

---

## 7. 阅读顺序与下一步

```
Part2 Day02 Phong（V·R 高光）
  ↓
Part4 Day11 抗锯齿
  ↓
Part5 Day01 高级光照（本文）← 半程向量，去掉高光断层
  ↓
Part5 Day02 Gamma 校正
```

---

## 8. 参考文献

1. LearnOpenGL CN — [高级光照](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/01%20Advanced%20Lighting/)
2. LearnOpenGL EN — [Advanced Lighting](https://learnopengl.com/Advanced-Lighting/Advanced-Lighting)
3. 《Part2 Day02 — 基础光照与法线》— Phong 三项与 `V·R`
4. 本工程 `src/cppfile/main.cpp` — 地板上切换 Phong / Blinn-Phong
