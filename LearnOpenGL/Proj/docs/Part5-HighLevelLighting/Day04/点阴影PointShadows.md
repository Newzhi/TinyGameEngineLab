# 点阴影 Point Shadows（Day 04）

对应 LearnOpenGL「[点阴影](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/03%20Shadows/02%20Point%20Shadows/)」：Day03 的阴影贴图是从**一个方向**拍的 2D 深度图，只适合定向光。点光四面八方都要出影，一张平面不够。改成立方体贴图：六个面各存一方向的最近深度，比较时用「片元 − 灯」当方向去采样。也叫 **万向阴影贴图**。

| 文档 | 内容 |
|------|------|
| [Day03 阴影映射](../Day03/阴影映射ShadowMapping.md) | 2D 深度图、光源空间比较、bias / PCF |
| [Part4 Day06 立方体贴图](../../Part4-HighLevelOpenGL/Day06/立方体贴图Cubemaps.md) | 六个面、方向采样 |
| [Part4 Day09 几何着色器](../../Part4-HighLevelOpenGL/Day09/几何着色器GeometryShader.md) | `EmitVertex`、一次进多个出 |
| **本文档** | 深度立方体、`gl_Layer`、线性距离、点光 PCF |

**工程对照**：当前 `main.cpp` 是房间（从内部看的大立方体）+ 若干箱子，点光在中央并沿 Z 缓缓摆动。`1` 无阴影；`2` 硬阴影（默认）；`3` 20 方向 PCF；`4` 把立方体深度显示成灰度（检查 Pass 1）。

**前置**：Day03 的两趟比较；立方体贴图怎么采样；几何着色器的 `layout` / `EndPrimitive`。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Point / omnidirectional shadows | **点阴影 / 万向阴影** | 点光四周都有影子 |
| Depth cubemap | **深度立方体贴图** | 六个面都是深度，不是颜色 |
| `gl_Layer` | **输出层** | 几何着色器指定当前图元写到立方体的哪一面 |
| `glFramebufferTexture` | **整张立方体挂到 FBO** | 不必每面单独 `Texture2D` 挂一次 |
| Linear light distance | **线性光距** | 片元到着的欧氏距离 / `far_plane`，当深度写入 |
| `samplerCube` | **立方体采样器** | 用方向向量取某一个面的 texel |

---

## 1. 为什么 2D 阴影贴图不够

Day03：灯在场景外头，光线近似平行，一张正交深度图就能盖住「灯朝这边看」的整块。点光在房间中间，箱子会在**四面墙、天花板、地板**上都投下影子。一张 2D 图只有一个视锥，背面、侧面全是盲区。

做法不变，还是「灯看见的最近深度 vs 这个片元离灯多远」。变的是深度容器：六个面拼成立方体，覆盖 360°。

可以渲染场景 6 次，每次换一个面、换一个 `lookAt`。本 Demo 跟官网一样，用几何着色器一次画出六个面：每个三角形进 GS，乘上 6 份 `shadowMatrices[face]`，设 `gl_Layer = face` 再发出去。

---

## 2. 深度立方体 FBO

六个面都是 `GL_DEPTH_COMPONENT`，尺寸相同（本 Demo 1024）：

```cpp
glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
for (unsigned int i = 0; i < 6; ++i)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// T、R 同样 CLAMP_TO_EDGE；过滤 NEAREST
```

GS 要往**整张**立方体写，不能只挂一个面：

```cpp
glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);
```

`glFramebufferTexture`（不是 `Texture2D`）把立方体作为分层附件；GS 里的 `gl_Layer` 选层。

---

## 3. 六个光空间矩阵

每个面：同样的 **90° 透视** × 不同的 `lookAt`。90° 才能让相邻面在棱上对齐、铺满立方体的一个面。`aspect` 必须是 1（宽=高）。

```cpp
glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, near, far);
// 右、左、上、下、前、后：target = lightPos + 轴向，up 必须按官网那组
```

`up` 不能随便填 `(0,1,0)`。上下两个面的 up 分别是 `(0,0,1)` / `(0,0,-1)`，否则面会拧、接缝错位。本 Demo 把官网那 6 个 `lookAt` 原样抄进 `buildShadowTransforms`。

`near` / `far` 要包住房间。比较阶段用的 `far_plane` 必须和这里同一个数：深度 Pass 把距离除以它写进 `[0,1]`，采样后再乘回来。

---

## 4. 深度 Pass 三个着色器

**顶点**：只乘 `model`，世界坐标交给 GS。

**几何**：`layout (triangles) in`，`triangle_strip, max_vertices=18` 出。对 `face = 0..5`：

```
gl_Layer = face
对三角形三个点：
    FragPos    = 世界坐标（后面算距离用）
    gl_Position = shadowMatrices[face] * FragPos
    EmitVertex
EndPrimitive
```

一个三角形变成六个，分别写到立方体六个面。

**片元**：不用默认的非线性 `gl_FragCoord.z`，自己写灯到片元的**直线距离**：

```
d = length(FragPos.xyz - lightPos) / far_plane
gl_FragDepth = d
```

采样时同样用 `length(fragPos - lightPos)` 去比，两边都是线性距离，不必再做透视除法、也不必 `*0.5+0.5` 当 UV——立方体用方向向量采样。

---

## 5. 第二趟：方向采样

场景顶点着色器不再输出 `FragPosLightSpace`。片元里：

```
fragToLight   = FragPos - lightPos
closestDepth  = texture(depthCubemap, fragToLight).r * far_plane
currentDepth  = length(fragToLight)
shadow        = (currentDepth - bias > closestDepth)
lighting      = ambient + (1-shadow)*(diffuse+specular)
```

`texture(samplerCube, dir)` 不要求 `dir` 单位化。环境光仍不乘阴影。

硬阴影一个 `bias`（本 Demo 0.05）。PCF 不能只在 xy 上 3×3：方向在 3D 里。朴素三重循环 4³=64 次太贵。官网改成 **20 个大致分开的偏移方向**，绕 `fragToLight` 略偏再采，除以 20。`diskRadius` 可随视距变大，远处影子更软。

模式 4：把 `closestDepth / far_plane` 当灰度输出。墙外若也有一圈暗部，说明立方体深度生成是对的。

---

## 6. 和本工程怎么接

### 6.1 画面与按键

点光在房间中央，沿 Z 按正弦摆动，六面墙上都能看到跟着走的影子。从内部看房间，所以画外壳时关掉面剔除，并 `reverse_normals` 把法线翻过来，否则内侧全是背光。

| 键 | 作用 |
|----|------|
| `1` | 无阴影 |
| `2`（默认） | 硬阴影 + 固定 bias |
| `3` | 20 方向 PCF |
| `4` | 灰度显示立方体线性深度 |
| WASD、鼠标、滚轮、Esc | 与前面几章相同 |

### 6.2 每一帧

1. 用当前 `lightPos` 建 6 个 `shadowProj * lookAt`。
2. `viewport(1024)` → 深度 FBO → VS+GS+FS 一次画出六个面。
3. `viewport(窗口)` → 场景着色器，绑 `samplerCube`，比较线性距离。
4. 画灯立方体（不进深度 Pass）。

相关文件：`src/cppfile/main.cpp`，`shaders/Part5-HighLevelLighting/Day04-PointShadows/`。

GS 一次画六面不一定比循环 6 次更快，驱动和场景而异。本工程跟官网走 GS，写法更短。

---

## 7. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 只有一面有影 / 接缝错位 | `lookAt` 的 up 和官网不一致；或 FOV 不是 90° |
| 全场没影或全黑 | `far_plane` 深度 Pass 和场景 Pass 不是同一个；或没绑 `samplerCube` |
| FBO 不完整 | 用了 `glFramebufferTexture2D` 只挂一个面，GS 的 `gl_Layer` 无处可写 |
| 房间内侧全黑 | 没 `reverse_normals`，或画房间时开着面剔除 |
| 影子跟着灯「黏」在表面上 | bias 太小；PCF 可略加大 |
| 模式 4 不是灰度渐变 | `closestDepth` 还停在 `[0,1]` 却又除了一次 `far_plane`，或忘了乘回来 |
| GS 编译失败 | `max_vertices` 小于 18；或 `shadowMatrices[6]` 没上传 |

---

## 8. 小练习

1. 点光为什么不能沿用 Day03 那张 2D 深度图？缺的是覆盖范围还是比较公式？
2. 深度为什么改成 `length / far_plane`，而不是 `gl_FragCoord.z`？
3. 采样立方体时为什么传 `fragPos - lightPos` 而不是世界坐标？
4. `gl_Layer` 和 `glFramebufferTexture` 各负责什么？少了哪一个会怎样？
5. （思考）六次独立绘制 vs 一次 GS，带宽和 GS 扩增哪个更可能成为瓶颈？

---

## 9. 阅读顺序与下一步

```
Part5 Day03 阴影映射（2D、定向光）
  ↓
Part5 Day04 点阴影（本文）← 深度立方体 + 线性距离
  ↓
[Part5 Day05 法线贴图](../Day05/法线贴图NormalMapping.md)
```

---

## 10. 参考文献

1. LearnOpenGL CN — [点阴影](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/03%20Shadows/02%20Point%20Shadows/)
2. LearnOpenGL EN — [Point Shadows](https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows)
3. 《Part5 Day03 — 阴影映射》— 比较公式与 bias
4. 《Part4 Day06 / Day09》— 立方体采样、几何着色器
5. 本工程 `src/cppfile/main.cpp` — 点光摆动 + cubemap 深度 + 20 方向 PCF
