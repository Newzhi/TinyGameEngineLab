# 阴影映射 Shadow Mapping（Day 03）

对应 LearnOpenGL「[阴影映射](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/03%20Shadows/01%20Shadow%20Mapping/)」：光被挡住的地方是阴影。实时光栅化没有完美算法，游戏里最常用的近似是 **阴影映射**：先从灯的视角把最近深度存进一张图，再画场景时拿当前片元的深度去比，更远的就是被挡住了。

| 文档 | 内容 |
|------|------|
| [Day02 Gamma 校正](../Day02/Gamma校正GammaCorrection.md) | 线性空间里算光 |
| [Part4 Day01 深度测试](../../Part4-HighLevelOpenGL/Day01/深度测试DepthTesting.md) | 深度缓冲、非线性 z |
| [Part4 Day05 帧缓冲](../../Part4-HighLevelOpenGL/Day05/帧缓冲Framebuffers.md) | 离屏 FBO、深度附件 |
| **本文档** | 深度贴图、光源空间、阴影比较、偏移 / PCF |
| [Day04 点阴影](../Day04/点阴影PointShadows.md) | 深度立方体、万向阴影 |

**工程对照**：当前 `main.cpp` 是地板 + 三只箱子 + 一盏定向光（用虚拟位置做 `lookAt`）。先渲染 1024² 深度贴图，再画场景。`1` 只光照无阴影；`2` 硬阴影（默认）；`3` 3×3 PCF 软边；`4` 把深度贴图铺到全屏。

**前置**：深度测试、FBO、正交投影、Blinn-Phong。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Shadow mapping | **阴影映射** | 从灯看过去：看得见就亮，被挡住就在影里 |
| Depth map / shadow map | **深度贴图 / 阴影贴图** | 灯视角下每个方向「最近一片」的深度纹理 |
| Light space | **光源空间** | 用灯的 view × projection 变过去的坐标 |
| `lightSpaceMatrix` | **光源空间矩阵** | `ortho × lookAt`，把世界坐标变到灯的裁剪空间 |
| Shadow acne | **阴影失真 / 摩尔纹** | 同一 texel 覆盖多个片元，深度比来比去在表面上来回跳 |
| Shadow bias | **阴影偏移** | 比较前把当前深度略减一点，避免自己挡住自己 |
| Peter panning | **阴影悬浮** | 偏移太大，影子离开物体底部 |
| Over-sampling | **过采样** | 灯视锥外的 UV / z 采到错误深度，大片误判成阴影 |
| PCF | **百分比渐进滤波** | 邻域多次比较再平均，影子边缘变软 |
| Directional light | **定向光** | 光线平行；阴影 Pass 用正交投影 |

---

## 1. 思路：从灯那儿看谁最近

灯和地板之间有一只箱子。从灯沿光线看过去，先碰到箱子，箱子后面的地板就是影。

真去打成千上万条射线太贵。换成你们已经用过的深度缓冲：把相机放到灯上，只把深度写进一张纹理。这张图里每个像素是「这个方向上最近的表面有多远」。

第二趟按正常相机画场景。把片元变到光源空间，用它的 xy 去采样深度贴图得到 `closestDepth`，用它的 z 当 `currentDepth`：

```
currentDepth > closestDepth  →  有东西更近，当前片元在影里
```

定向光没有真实位置，但做阴影映射必须有一个**虚拟相机**：沿着光的方向、放在场景外头某处，用正交投影（光线平行、不透视变形）。

两趟流程：

```
Pass 1  视口 = 阴影贴图分辨率
        绑 depth FBO，只清深度
        lightSpaceMatrix 画全场 → 写出 depthMap

Pass 2  视口 = 窗口
        绑默认 FBO
        正常 MVP 画全场，采样 depthMap 做比较
```

两趟分辨率经常不一样，**每次都要 `glViewport`**。忘了改，深度图会缺一块或只填一角。

---

## 2. 深度贴图 FBO

只要深度，不要颜色。纹理内部格式 `GL_DEPTH_COMPONENT`，挂到 `GL_DEPTH_ATTACHMENT`。没有颜色附件的 FBO 不完整，必须显式关掉颜色读写：

```cpp
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);
```

分辨率本 Demo 用 1024。越大锯齿越少，也越占带宽。

光源空间矩阵：

```cpp
glm::mat4 lightProjection = glm::ortho(-10.f, 10.f, -10.f, 10.f, 1.f, 7.5f);
glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
glm::mat4 lightSpaceMatrix = lightProjection * lightView;
```

正交盒子要包住需要出影的物体。漏在盒子外面的东西不会进深度图，也就不会投影子。`near` / `far` 太紧会裁掉灯前或灯后的几何。

深度 Pass 的顶点着色器只做：

```
gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
```

片元着色器可以是空的：没有颜色输出，驱动照样写 `gl_FragCoord.z`。

调试时把深度图铺到四边形上，正交投影下灰度就是 0～1 的线性深度（近黑远白）。透视投影的深度是非线性的，要先按 Day01 的公式拉直才能看清，比较逻辑本身不用改。

---

## 3. 第二趟：比较出阴影

场景顶点着色器除了普通 MVP，还要把世界坐标变到光源空间，交给片元：

```
FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0)
```

片元里 `ShadowCalculation`：

```
1. projCoords = FragPosLightSpace.xyz / w     // 透视除法；正交时 w=1，留着两种投影都能用
2. projCoords = projCoords * 0.5 + 0.5       // NDC [-1,1] → UV/深度 [0,1]
3. closestDepth = texture(shadowMap, projCoords.xy).r
4. currentDepth = projCoords.z
5. shadow = currentDepth > closestDepth ? 1 : 0
```

光照仍是 Blinn-Phong。**只有漫反射和高光乘 `(1 - shadow)`**，环境光不乘，否则影里全黑、不真实。

```
lighting = (ambient + (1 - shadow) * (diffuse + specular)) * albedo
```

---

## 4. 三处瑕疵

### 4.1 阴影失真（Acne）

深度图一个 texel 是一块斜坡。远处多个片元采到同一个深度：有的「在坡上」、有的「在坡下」，地板就会拉出条纹。

比较前加偏移：

```
bias = max(0.05 * (1.0 - dot(N, L)), 0.005)
shadow = (currentDepth - bias) > closestDepth
```

表面越斜，偏移越大。值要按场景微调：太小还有条纹，太大影子会离开物体。

### 4.2 阴影悬浮（Peter Panning）

偏移把物体「变薄」了，影子和底边脱开。深度 Pass 对实心封闭体做**正面剔除**，只用背面深度，偏移就不那么容易把底边抬起来。地板是单面，正面剔除会整块消失，本 Demo 只对箱子开 `glCullFace(GL_FRONT)`。

### 4.3 过采样（视锥外）

`GL_REPEAT` 会把灯视锥外的 UV 折回来，远处一整片误判成影。改成 `GL_CLAMP_TO_BORDER`，边界颜色 `(1,1,1,1)`：视锥外采到深度 1，比谁都远，不算影。

`projCoords.z > 1` 表示超出远平面，钳制管不到 z，要单独把 `shadow` 置 0。结果是：**只有深度图盖到的范围才有影子**，远景保持亮，比整片涂黑合理。

---

## 5. PCF

硬阴影边缘一块一块的，因为多个片元共享同一个深度 texel。简单 PCF：在周围 3×3 各采一次、各自比较，再除以 9。

```
texelSize = 1.0 / textureSize(shadowMap, 0)
对 x,y ∈ [-1,1]：
    采样 proj.xy + vec2(x,y) * texelSize
    累加 (currentDepth - bias > pcfDepth)
shadow /= 9
```

远处看边缘软很多。加大核或 texel 偏移会更糊，也更贵。本 Demo 模式 3 就是这 9 次。

定向光用正交；点光 / 聚光更适合透视（下一章点阴影）。比较公式两种投影一样。

---

## 6. 和本工程怎么接

### 6.1 画面与按键

木纹地板，三只箱子（一只悬空，方便看出「没影子就分不清谁在上」）。灯在 `(-2, 4, -1)`，小白立方体标虚拟位置。

| 键 | 作用 |
|----|------|
| `1` | 无阴影，只有 Blinn-Phong |
| `2`（默认） | 硬阴影 + 斜率偏移 + 边界钳制 |
| `3` | 再加 3×3 PCF |
| `4` | 全屏显示深度贴图（正交，近黑远白） |
| WASD、鼠标、滚轮、Esc | 与前面几章相同 |

### 6.2 每一帧

1. `viewport(1024,1024)` → 深度 FBO → `shadowDepth` + `lightSpaceMatrix` 画地板和箱子（箱子正面剔除）。
2. `viewport(窗口)` → FBO 0 → `shadow` 着色器，绑 albedo 与 `depthMap`，按 `uShadowMode` 比较。
3. 画灯立方体（不进阴影 Pass）。
4. 模式 4：全屏四边形采样 `depthMap`。

相关文件：`src/cppfile/main.cpp`，`shaders/Part5-HighLevelLighting/Day03-ShadowMapping/shadowDepth.*`，`shaders/Part5-HighLevelLighting/Day03-ShadowMapping/shadow.*`，`shaders/Part5-HighLevelLighting/Day03-ShadowMapping/debugDepth.*`。

---

## 7. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 全场没影子 / 深度图是空的 | 忘了 `glViewport` 改成 1024；或 `lightSpace` 的 ortho 盒子没包住场景 |
| FBO 不完整 | 没 `glDrawBuffer(GL_NONE)` / `glReadBuffer(GL_NONE)` |
| 地板摩尔纹 | 没 bias，或 bias 太小。先看模式 2 |
| 箱子底下影子离开地面 | bias 太大，或深度 Pass 没对箱子正面剔除 |
| 视锥外一大片黑 | wrap 还是 `GL_REPEAT`，或没处理 `z > 1` |
| 影子边缘锯齿块 | 硬采样；切模式 3，或提高 `SHADOW_WIDTH` |
| 模式 4 全白 / 全黑 | 正交下深度集中在某一段。走近灯方向看箱子轮廓；透视深度才需要线性化 |
| 第二趟画面只在一角 | Pass 1 改了 viewport，Pass 2 没改回窗口大小 |

---

## 8. 小练习

1. 为什么深度 Pass 可以几乎空着片元着色器，深度图却仍然有数据？
2. `FragPosLightSpace.xyz / w` 在正交投影下有没有效果？为什么还要写？
3. 环境光为什么不乘 `(1 - shadow)`？
4. 斜率偏移 `max(0.05*(1-N·L), 0.005)` 在地板和箱子侧面各偏向哪一端？
5. （思考）若灯绕场景转，`lightView` 每帧重建，深度 Pass 能不能省？和静态灯有何不同？

---

## 9. 阅读顺序与下一步

```
Part5 Day02 Gamma 校正
  ↓
Part5 Day03 阴影映射（本文）← 灯视角深度图 + 比较
  ↓
Part5 Day04 点阴影（深度立方体）
```

---

## 10. 参考文献

1. LearnOpenGL CN — [阴影映射](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/03%20Shadows/01%20Shadow%20Mapping/)
2. LearnOpenGL EN — [Shadow Mapping](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping)
3. 《Part4 Day01 — 深度测试》— 缓冲里的 z、透视非线性
4. 《Part4 Day05 — 帧缓冲》— 离屏附件；本章把颜色附件换成纯深度纹理
5. 本工程 `src/cppfile/main.cpp` — 双 Pass + PCF + 深度可视化
