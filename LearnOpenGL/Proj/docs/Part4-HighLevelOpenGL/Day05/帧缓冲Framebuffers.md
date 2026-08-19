# 帧缓冲 Framebuffers（Day 05）

对应 LearnOpenGL「[帧缓冲](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/05%20Framebuffers/)」：自己创建一块「画布」（颜色 / 深度 / 模板），先离屏画场景，再把颜色纹理贴到全屏四边形上做**后处理**。

| 文档 | 内容 |
|------|------|
| [Day01～Day04](../README.md) | 深度 / 模板 / 混合 / 面剔除 |
| **本文档** | FBO 创建、附件、完整性、离屏渲染、后处理；与延迟渲染的区别 |
| 后续 | 立方体贴图、高级数据 / 延迟着色（进阶）…… |

**工程对照**：当前 `main.cpp` 为帧缓冲 Demo——场景画到自定义 FBO 的颜色纹理，再全屏四边形采样；按键切换后处理（原图 / 反相 / 灰度 / 锐化核）。

**前置**：纹理采样、VAO 画四边形、默认帧缓冲（窗口）的直觉。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Framebuffer / FBO | **帧缓冲对象** | 颜色+深度+模板等附件的集合；默认 FBO 由窗口创建 |
| Attachment | **附件** | 挂到 FBO 上的颜色图 / 深度缓冲等 |
| Color attachment | **颜色附件** | FBO 上「存片元颜色」的那一块；可挂纹理或 RBO |
| Texture color attachment | **纹理颜色附件** | 用一张 2D 纹理充当颜色附件；可再被着色器采样 |
| Renderbuffer Object (RBO) | **渲染缓冲对象** | 只写、不可当纹理采样的附件；适合深度/模板 |
| Off-screen rendering | **离屏渲染** | 画到自定义 FBO，不直接出现在窗口 |
| Post-processing | **后处理** | 对整幅屏幕纹理再跑一遍片元着色器 |
| Completeness | **完整性** | FBO 能否真正用来渲染的检查结果 |

---

## 1. 帧缓冲是什么？

到目前为止你一直在画**默认帧缓冲**（GLFW 创建窗口时生成）：

- 颜色缓冲 → 你看见的像素  
- 深度缓冲 → 远近测试  
- 模板缓冲 → 标签遮罩  

OpenGL 允许再建一个 **FBO**，自己挂颜色纹理、深度/模板附件。  
流程就变成：

```
场景 Draw  →  自定义 FBO（颜色纹理里存下「整屏画面」）
                ↓
全屏四边形采样该纹理 → 片元着色器改颜色（后处理）
                ↓
画到默认帧缓冲（窗口）→ SwapBuffers 显示
```

镜子、UI、Bloom、反相滤镜都建立在这个「先离屏、再采样」上。

---

## 2. 创建与绑定（关键代码）

```cpp
unsigned int fbo;
glGenFramebuffers(1, &fbo);
glBindFramebuffer(GL_FRAMEBUFFER, fbo);   // 之后读写都针对这个 FBO

// ……挂附件（见下）……

if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "ERROR::FRAMEBUFFER incomplete\n";

glBindFramebuffer(GL_FRAMEBUFFER, 0);     // 回到默认帧缓冲（窗口）
// 不用时：
glDeleteFramebuffers(1, &fbo);
```

| 绑定目标 | 含义 |
|---------|------|
| `GL_FRAMEBUFFER` | 读+写都绑到该 FBO（最常用） |
| `GL_DRAW_FRAMEBUFFER` | 只影响绘制 / Clear |
| `GL_READ_FRAMEBUFFER` | 只影响 `glReadPixels` 等读取 |

**完整 FBO** 至少要满足：有颜色附件、附件已分配内存、样本数一致等。  
检查：`glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE`。

---

## 3. 附件怎么挂？

FBO 本身只是一个「架子」，真正存像素的是挂上去的**附件**。  
常见三类槽位：

| 槽位 | 枚举例子 | 存什么 |
|------|----------|--------|
| 颜色 | `GL_COLOR_ATTACHMENT0`（还可 1、2…） | 片元最终颜色 `FragColor` |
| 深度 | `GL_DEPTH_ATTACHMENT` | 深度测试用的远近 |
| 模板 | `GL_STENCIL_ATTACHMENT` | 模板标签 |
| 深度+模板合一 | `GL_DEPTH_STENCIL_ATTACHMENT` | 24 位深度 + 8 位模板 |

每个槽位既可以挂**纹理**，也可以挂 **RBO**。后处理需要「整屏当贴图再画一遍」，所以颜色槽几乎总是挂纹理。

### 3.1 什么是「纹理颜色附件」？

拆开三个词：

1. **颜色**：片元着色器输出的 RGB(A)，平时写进窗口的颜色缓冲。  
2. **附件**：这块颜色缓冲不再是窗口自带的，而是你挂到自定义 FBO 上的一张「纸」。  
3. **纹理**：这张纸的实现形式是 `GL_TEXTURE_2D`，所以之后还能 `texture(sampler, uv)` 读回来。

合起来：

```
纹理颜色附件 =
  「一张普通 2D 纹理」
  + 被 glFramebufferTexture2D 挂到 FBO 的 COLOR_ATTACHMENTn
  → 绑这个 FBO 再 Draw 时，场景颜色写进这张纹理的像素里
  → 解绑 FBO 后，把它当普通贴图绑到采样器，就能做后处理
```

和「从文件加载的箱子贴图」对比：

| | `container2.png` 纹理 | 纹理颜色附件 |
|--|----------------------|--------------|
| 数据从哪来 | `stbi_load` 读磁盘 | 离屏渲染时 GPU 写进去 |
| 创建时 data | 图片字节 | 常传 `NULL`，只先占显存 |
| 用途 | 贴在模型表面 | 整帧场景图 / G-Buffer 某一层 |
| 是否可采样 | 是 | **是**（这正是选纹理而不是 RBO 的理由） |

直觉图：

```
绑 FBO 画箱子/地板
        │
        ▼
┌─────────────────────┐
│  textureColorbuffer  │  ← 这就是纹理颜色附件
│  （一张 RGB 图）      │
│  每个 texel = 屏幕上 │
│  某个像素的颜色      │
└─────────────────────┘
        │
        ▼  Pass2：当 screenTexture 采样
全屏四边形 → 反相 / 灰度 / 锐化 → 画到窗口
```

本 Demo 里对应变量：`textureColorbuffer`，挂在 `GL_COLOR_ATTACHMENT0`。

### 3.2 怎么创建并挂上（关键代码）

```cpp
unsigned int colorTex;
glGenTextures(1, &colorTex);
glBindTexture(GL_TEXTURE_2D, colorTex);
// data=NULL：只分配显存，内容由之后的离屏渲染写入
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
             SCR_WIDTH, SCR_HEIGHT, 0,
             GL_RGB, GL_UNSIGNED_BYTE, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// 把这张纹理挂到当前 FBO 的「第 0 号颜色槽」
glFramebufferTexture2D(
    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
    GL_TEXTURE_2D, colorTex, 0);
```

`glFramebufferTexture2D` 参数含义：

| 参数 | 本例 | 含义 |
|------|------|------|
| target | `GL_FRAMEBUFFER` | 挂到当前绑定的 FBO |
| attachment | `GL_COLOR_ATTACHMENT0` | 第 0 个颜色槽（可有多个，延迟渲染会用到） |
| textarget | `GL_TEXTURE_2D` | 附件是 2D 纹理 |
| texture | `colorTex` | 要挂的纹理 id |
| level | `0` | mipmap 第 0 级 |

之后在屏幕着色器里：

```glsl
uniform sampler2D screenTexture;
FragColor = texture(screenTexture, TexCoords); // 采到的就是整帧离屏结果
```

CPU 侧须先 `glBindTexture(GL_TEXTURE_2D, colorTex)`，且 `setInt("screenTexture", 0)` 与 `GL_TEXTURE0` 对应。

> 若离屏纹理尺寸 ≠ 窗口，绑 FBO 前要 `glViewport(0,0, texW, texH)`，切回窗口再改回窗口尺寸。

### 3.3 深度 / 模板 = RBO（只要测试、不采样）

```cpp
unsigned int rbo;
glGenRenderbuffers(1, &rbo);
glBindRenderbuffer(GL_RENDERBUFFER, rbo);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                      SCR_WIDTH, SCR_HEIGHT);
glFramebufferRenderbuffer(
    GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
    GL_RENDERBUFFER, rbo);
```

| 需求 | 更合适 |
|------|--------|
| 后处理要采样颜色 | **纹理颜色附件** |
| 只要深度/模板测试 | **RBO**（更快、不能当 sampler） |
| 也要采样深度（软阴影等） | 深度也做成**纹理**附件 |

颜色也可以挂成 RBO，但那样就**不能** `texture()` 做后处理，本章不会这么做。

---

## 4. 渲染到纹理的标准流程

```cpp
// ---------- Pass 1：场景 → FBO ----------
glBindFramebuffer(GL_FRAMEBUFFER, fbo);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glEnable(GL_DEPTH_TEST);
// 用普通场景 shader 画地板、箱子……
// （写入的是 colorTex + rbo）

// ---------- Pass 2：屏幕四边形 → 默认 FBO ----------
glBindFramebuffer(GL_FRAMEBUFFER, 0);
glClear(GL_COLOR_BUFFER_BIT);          // 深度一般可不测了
glDisable(GL_DEPTH_TEST);              // 全屏四边形不需要深度

screenShader.use();
glBindTexture(GL_TEXTURE_2D, colorTex); // 采样 Pass1 的结果
glBindVertexArray(quadVAO);
glDrawArrays(GL_TRIANGLES, 0, 6);
```

全屏四边形顶点（NDC，覆盖整个屏幕）：

```cpp
float quadVertices[] = {
    // pos        // uv
    -1.f,  1.f,   0.f, 1.f,
    -1.f, -1.f,   0.f, 0.f,
     1.f, -1.f,   1.f, 0.f,

    -1.f,  1.f,   0.f, 1.f,
     1.f, -1.f,   1.f, 0.f,
     1.f,  1.f,   1.f, 1.f
};
```

屏幕 VS 只需把 NDC 位置写出，并把 UV 传给 FS：

```glsl
// framebufferScreen.vs
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;
void main() {
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
```

---

## 5. 如何改渲染纹理做后处理？（重点）

后处理 = **只改全屏片元着色器**，颜色附件仍是同一张 `colorTex`。

### 5.1 原图（直通）

```glsl
vec4 color = texture(screenTexture, TexCoords);
FragColor = color;
```

### 5.2 反相

```glsl
FragColor = vec4(vec3(1.0 - texture(screenTexture, TexCoords)), 1.0);
```

### 5.3 灰度

```glsl
vec3 c = texture(screenTexture, TexCoords).rgb;
float g = 0.2126*c.r + 0.7152*c.g + 0.0722*c.b;
FragColor = vec4(vec3(g), 1.0);
```

### 5.4 核效果（锐化 / 模糊 / 边缘）

用周围 9 个像素加权求和。偏移量 = `1.0 / 纹理尺寸`（或固定 `1.0/300` 作近似）：

```glsl
const float offset = 1.0 / 300.0;
vec2 offsets[9] = vec2[](
    vec2(-offset,  offset), vec2(0,  offset), vec2(offset,  offset),
    vec2(-offset,  0),     vec2(0,  0),     vec2(offset,  0),
    vec2(-offset, -offset), vec2(0, -offset), vec2(offset, -offset)
);
// 锐化核示例
float kernel[9] = float[](
    -1, -1, -1,
    -1,  9, -1,
    -1, -1, -1
);
vec3 sampleTex[9];
for (int i = 0; i < 9; i++)
    sampleTex[i] = texture(screenTexture, TexCoords + offsets[i]).rgb;
vec3 col = vec3(0.0);
for (int i = 0; i < 9; i++)
    col += sampleTex[i] * kernel[i];
FragColor = vec4(col, 1.0);
```

改 `kernel[]` 即可在**不改 FBO、不改场景**的情况下换滤镜。

### 5.5 本 Demo 的切换方式

| 键 | 效果 | 对应 FS 逻辑 |
|----|------|--------------|
| `1` | 原图 | 直通采样 |
| `2` | 反相 | `1.0 - color` |
| `3` | 灰度 | 加权亮度 |
| `4` | 锐化 | 3×3 核 |

CPU 侧用 `uniform int uEffect` 传模式即可，或为每种效果准备不同 FS。

---

## 6. 纹理 vs RBO 怎么选？

```
需要在着色器里 texture() 采样吗？
   ├─ 是 → 做成 Texture 附件（颜色几乎总是纹理）
   └─ 否 → 用 RBO 往往更合适（深度+模板很常见）
```

深度若只要测试、不要采样 → RBO `GL_DEPTH24_STENCIL8`。  
这正是官网「渲染到纹理」示例的标配组合。

---

## 7. 帧缓冲 vs 延迟渲染（Deferred Shading）

二者经常被一起提起，但**层级不同**，不要等同。

### 7.1 一句话区分

| | 帧缓冲（Framebuffer / FBO） | 延迟渲染（Deferred Rendering / Deferred Shading） |
|--|------------------------------|------------------------------------------------------|
| 是什么 | OpenGL 的 **基础设施**：一块可自定义的「画布」 | 一种 **渲染管线策略**：先存几何信息，再算光照 |
| 解决什么 | 「画到哪」——默认窗口以外的离屏目标 | 「光照怎么算」——大量灯光时避免对每个片元重复算可见几何 |
| 和本章关系 | 本章正在学的 API / 对象 | 以后高级光照章节会用到；**实现时几乎一定要用 FBO** |

关系可以记成：

```
帧缓冲（FBO）  =  工具 / 画布
延迟渲染        =  用多张画布实现的一种光照算法
后处理（本章）  =  用一张（或几张）颜色画布做全屏滤镜
```

**有 FBO ≠ 就是延迟渲染。**  
本章 Demo：1 张颜色纹理 + 全屏滤镜 = 前向渲染 + 后处理。  
延迟渲染：通常要 **多张颜色附件（G-Buffer）**，存位置、法线、反照率等，再另 Pass 读这些纹理算光照。

### 7.2 流程对比（直觉）

**前向渲染（你们一直在用）+ 本章后处理：**

```
对每个物体：
  VS → FS（采样贴图 + 算光照）→ 颜色写进缓冲
        ↓
（可选）整屏采样颜色纹理 → 反相 / 灰度 / 锐化
```

灯光很多时：每个物体 × 每盏灯都在 FS 里算一遍，重叠片元也会重复算光。

**延迟渲染（典型两阶段）：**

```
Pass Geometry（Geometry Pass）：
  场景几何 → FBO 的多张附件（G-Buffer）
  例如：世界位置、法线、Albedo、粗糙度……
  这一遍通常不算最终光照，或只做极少处理

Pass Lighting（Lighting Pass）：
  全屏四边形（或光体积）采样 G-Buffer
  对「屏幕上每个可见像素」按灯光列表算光照
  → 写出最终颜色（再可接 Bloom 等后处理）
```

灯光数量大时，光照主要在「屏幕像素 × 灯」上做，而不是「被光栅化的片元 × 灯」重复爆炸。

### 7.3 附件用法上的差别

| | 本章后处理 FBO | 延迟渲染 G-Buffer |
|--|----------------|-------------------|
| 颜色附件数量 | 通常 **1** 张最终场景色 | **多张**（MRT：Multiple Render Targets） |
| 附件里存什么 | 已经照亮好的颜色 | 延迟到光照 Pass 才用的中间数据 |
| 深度 | RBO 或深度纹理，主要给几何 Pass 测试 | 常要深度纹理，光照/重建位置时会用 |
| 第二遍在干什么 | 滤镜（改观感） | **算光照**（改物理/着色结果） |

官网后续「延迟着色」章节会讲 `glDrawBuffers` 一次写出多个 `COLOR_ATTACHMENT0/1/2…`；本章先掌握「单颜色附件 + 采样」即可。

### 7.4 各自擅长什么

| 场景 | 更合适 |
|------|--------|
| 反相、模糊、边缘、UI 叠层、镜子 | **帧缓冲 + 后处理**（本章） |
| 几十上百盏动态灯、复杂材质延迟算光 | **延迟渲染**（建立在多附件 FBO 上） |
| 大量半透明 | 延迟渲染本身很麻烦（透明常仍用前向）；本章混合那套仍常保留 |
| 只要学会「离屏再画回屏幕」 | 帧缓冲足够，不必上延迟 |

### 7.5 和本 Demo 的对照

当前工程：

```
Pass1：前向画箱子/地板 → 1 张 RGB 颜色纹理
Pass2：采样这张纹理 → uEffect 滤镜
```

若将来做延迟，大致会变成：

```
Pass1：几何 → 多张纹理（位置/法线/Albedo…）
Pass2：采样这些纹理算光照 → 得到「已照亮」颜色
Pass3：（可选）再对最终颜色做本章这种后处理
```

所以：**延迟渲染用帧缓冲当载体；本章帧缓冲 Demo 不是延迟渲染，而是后处理。**

---

## 8. 和本工程怎么接

### 8.1 Demo 内容（当前 `main.cpp`）

1. 创建 FBO + `GL_COLOR_ATTACHMENT0`（RGB 纹理）+ 深度模板 RBO  
2. Pass1：画地板 + 两个箱子到 FBO  
3. Pass2：解绑 FBO，关深度，全屏四边形采样颜色纹理并按键后处理  

相关文件：

| 文件 | 作用 |
|------|------|
| `src/cppfile/main.cpp` | FBO 创建与双 Pass 流程 |
| `shaders/framebuffer.vs` / `framebuffer.fs` | 普通场景着色 |
| `shaders/framebufferScreen.vs` / `framebufferScreen.fs` | 全屏后处理 |
| `Resource/Texture/container.jpg` / `container2.png` | 地板 / 箱子 |

### 8.2 排错 checklist

| 现象 | 排查 |
|------|------|
| 黑屏 / 无画面 | FBO 是否完整？是否忘了 `BindFramebuffer(0)` 画屏幕？ |
| 只有一块局部有图 | 离屏时 `glViewport` 尺寸是否对？ |
| 后处理采样到空纹理 | Pass1 是否绑对了 FBO？颜色附件是否挂上？ |
| 深度穿帮 | FBO 是否挂了深度附件？Pass1 是否 `Enable(DEPTH_TEST)`？ |
| 全屏四边形被深度切掉 | Pass2 是否关了深度测试？ |

---

## 9. 小练习

1. 为何后处理要采样**纹理**颜色附件，而不是 RBO？  
2. Pass2 为什么常 `glDisable(GL_DEPTH_TEST)`？  
3. 把锐化核改成模糊核（平均 1/9），画面会怎样？  
4. 窗口 resize 后若不重建颜色纹理 / RBO，会出什么问题？  
5. （思考）一次要做模糊再锐化，是否需要两个 FBO（Ping-Pong）？  
6. 用自己的话区分：帧缓冲、后处理、延迟渲染三者各是什么层级？  

---

## 10. 阅读顺序与下一步

```
Day04 面剔除
  ↓
Day05 帧缓冲（本文）← FBO + 颜色纹理 + 后处理
  ↓
立方体贴图 / 高级数据 …
  ↓（更后）延迟着色 Deferred Shading ← 多附件 G-Buffer，建立在 FBO 之上
```

---

## 11. 参考文献

1. LearnOpenGL CN — [帧缓冲](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/05%20Framebuffers/)  
2. LearnOpenGL EN — [Framebuffers](https://learnopengl.com/Advanced-OpenGL/Framebuffers)  
3. LearnOpenGL CN — [延迟着色](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/08%20Deferred%20Shading/)（进阶，建立在 FBO / MRT 上）  
4. 《Part4 Day01 — 深度测试》— 默认帧缓冲里深度附件的角色  
