# 抗锯齿 Anti-Aliasing（Day 11）

对应 LearnOpenGL「[抗锯齿](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/11%20Anti%20Aliasing/)」：光栅化把连续边缘落在离散像素上，轮廓会呈锯齿（走样）。MSAA 用每个像素多个子采样点算覆盖率，边缘颜色按覆盖比例混合，看起来更平滑。

| 文档 | 内容 |
|------|------|
| [Day05 帧缓冲](../Day05/帧缓冲Framebuffers.md) | FBO、颜色纹理 / RBO、`glBlitFramebuffer` |
| [Day10 实例化](../Day10/实例化Instancing.md) | 上一章 |
| **本文档** | 走样、SSAA / MSAA、窗口 MSAA、离屏多重采样 FBO、还原 |

**工程对照**：当前 `main.cpp` 是立方体 MSAA Demo。窗口**不**请求 `GLFW_SAMPLES`，默认缓冲每像素 1 个样本。`1` 直接画到窗口（锯齿）；`2` 画到 4x 多重采样 FBO 再 blit 到窗口（默认，边缘平滑）；`3` 先还原成普通 2D 纹理，全屏四边形灰度采样（后处理必须先 resolve）。

**前置**：光栅化、Day05 FBO 附件与完整性、`glBlitFramebuffer`。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Aliasing | **走样** | 斜边落在像素网格上，轮廓一格一格的 |
| Anti-aliasing | **抗锯齿 / 反走样** | 让边缘看起来连续 |
| Sample point | **采样点** | 像素里用来判断「三角形有没有盖住我」的点 |
| SSAA | **超采样抗锯齿** | 用更高分辨率画完再缩小。效果好，片元多、很贵 |
| MSAA | **多重采样抗锯齿** | 每像素多个子样本算覆盖；每个图元每像素仍只跑一次片元着色器 |
| Subsample | **子采样点** | 一个像素里的 2/4/8 个覆盖测试点 |
| Multisample buffer | **多重采样缓冲** | 能存多个子样本颜色 / 深度 / 模板的附件 |
| Resolve | **还原** | 把多子样本平均成一张普通图像，才能拿去采样或显示 |
| `glBlitFramebuffer` | **帧缓冲位块传送** | 把一块矩形从读 FBO 拷到写 FBO；MSAA → 单样本时顺便还原 |
| `sampler2DMS` | **多重采样采样器** | 不还原、直接在着色器里取某个子样本。本 Demo 不用 |

---

## 1. 锯齿从哪来

光栅器把三角形变成像素。每个像素中心一个采样点：点在三角形**内部**才生成片元。边缘常常只盖住像素的一角，中心却在外面，这个像素就完全不画。斜边于是变成台阶。

放大立方体轮廓能看见一格一格的像素，这就是走样。

---

## 2. SSAA 和 MSAA

**SSAA**：用更高分辨率整屏画一遍（片元着色器次数按像素变多），再缩小到窗口。边缘信息来自多出来的像素。带宽和填充率都贵，现在很少当默认方案。

**MSAA**：每个像素放 N 个子采样点（本 Demo N=4）。

- 覆盖：几个子点落在三角形里，这个像素就「盖住了几分之几」。
- 着色：每个图元、每个像素的片元着色器仍只跑**一次**（数据按像素中心插值），不是每个子点跑一次。
- 写入：颜色按覆盖比例和帧缓冲里已有颜色混合。4 个子点里盖住 2 个，大约掺一半三角形颜色。
- 深度 / 模板：按子样本存储，缓冲大约大 N 倍。

三角形内部四个子点都被盖住，颜色写满。只有边缘像素才出现浅一号的过渡色，远看就是平滑边。

---

## 3. 窗口上的 MSAA（GLFW）

创建窗口前：

```cpp
glfwWindowHint(GLFW_SAMPLES, 4);
glEnable(GL_MULTISAMPLE);
```

GLFW 会给默认帧缓冲配 4 个子样本的颜色、深度、模板。光栅器在驱动里完成多重采样，场景代码不用改。

本 Demo **故意不设** `GLFW_SAMPLES`，默认缓冲仍是单样本，才能用按键对比「有 / 无 MSAA」。若在 `glfwInit` 之后加上 `GLFW_SAMPLES, 4`，模式 1 画到窗口也会变平滑，对比就没了。

---

## 4. 离屏 MSAA（自己建 FBO）

后处理、延迟管线都画到自定义 FBO。这时要自己建多重采样附件。

### 4.1 多重采样颜色纹理

```cpp
glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, width, height, GL_TRUE);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D_MULTISAMPLE, tex, 0);
```

目标是 `GL_TEXTURE_2D_MULTISAMPLE`，不是 `GL_TEXTURE_2D`。最后一个 `GL_TRUE`：各纹素用同一套子样本位置。

### 4.2 多重采样 RBO（深度 / 模板）

```cpp
glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, width, height);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
```

样本数必须和颜色附件一致，否则 FBO 不完整。

### 4.3 还原

多重采样图含多个子样本，**不能**当普通 `sampler2D` 去 `texture()`。先 blit 到单样本目标：

```cpp
glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFBO);
glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);   // 或另一个普通 FBO
glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
```

`GL_READ_FRAMEBUFFER` 是源，`GL_DRAW_FRAMEBUFFER` 是目标。源是 MSAA、目标是单样本时，这次拷贝就是 resolve。

要做后期处理：MSAA FBO → blit → 普通颜色纹理 FBO → 全屏四边形采样这张 2D 纹理。伪代码与官网一致。

边缘检测一类滤镜会在单样本纹理上重新做出锯齿。可以再模糊，或自己写抗锯齿。本 Demo 模式 3 只做灰度，轮廓仍是 MSAA 之后的平滑边。

样本越多越平滑，也越占带宽。教程写作时常用 4x，本 Demo 也是 4。

---

## 5. 自定义抗锯齿（了解即可）

不还原、直接在着色器里取子样本：

```glsl
uniform sampler2DMS screenTextureMS;
vec4 colorSample = texelFetch(screenTextureMS, ivec2(gl_FragCoord.xy), 3);
```

`texelFetch` 的第三个参数是子样本下标。大型引擎会在这里写自己的 resolve。本工程 3.3 支持 `sampler2DMS`，Demo 没有走这条路。

---

## 6. 和本工程怎么接

### 6.1 画面与按键

斜放的彩色立方体，对着轮廓看锯齿。WASD、鼠标、Esc 与前面几章相同。拉窗口会重建两套 FBO。

| 键 | 路径 | 轮廓 |
|----|------|------|
| `1` | 默认帧缓冲，单样本 | 锯齿 |
| `2`（默认） | 4x MSAA FBO → blit 到窗口 | 平滑 |
| `3` | MSAA FBO → blit 到 2D 纹理 → 全屏四边形灰度 | 平滑且变灰 |

### 6.2 初始化（对照 `main.cpp`）

1. 不设置 `GLFW_SAMPLES`。`glEnable(GL_DEPTH_TEST)`、`glEnable(GL_MULTISAMPLE)`。
2. `recreateFramebuffers`：多重采样颜色纹理 + `GL_DEPTH24_STENCIL8` RBO 挂到 `g_msaaFBO`；普通 RGB 纹理挂到 `g_resolveFBO`。
3. 立方体 VAO：位置 + 顶点色。全屏四边形 VAO：NDC 位置 + UV。

### 6.3 每一帧

- 模式 1：`glBindFramebuffer(..., 0)`，清屏，画立方体。
- 模式 2：绑 `g_msaaFBO` 画立方体，再 `READ=msaa / DRAW=0` 做 `glBlitFramebuffer`。
- 模式 3：同样画进 MSAA，blit 到 `g_resolveFBO`，关掉深度，四边形采样 `g_resolveColor`，`uGray = 1`。

相关文件：`src/cppfile/main.cpp`，`shaders/msaa.vs` / `msaa.fs`，`shaders/msaaScreen.vs` / `msaaScreen.fs`。

---

## 7. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 模式 1 也没有锯齿 | 创建窗口时设了 `GLFW_SAMPLES, 4`，默认缓冲已经是 MSAA |
| FBO 不完整 | 颜色和深度的样本数不一致；或用了 `GL_TEXTURE_2D` 去挂多重采样纹理 |
| 全屏四边形是黑的 / 采样失败 | 片元着色器对 MSAA 纹理用了 `sampler2D`。必须先 blit 到普通 2D |
| 拉窗口后画面拉伸或花掉 | FBO 尺寸还是旧的。应在 `framebuffer_size_callback` 里重建 |
| 模式 3 立方体消失 | blit 之后忘了 `glBindFramebuffer(0)`，或全屏 pass 没关深度测试 |
| 性能掉很多 | 样本数过大（8x、16x）。先用 4x |

---

## 8. 小练习

1. 像素中心一个采样点时，为什么斜边会缺像素？MSAA 的子采样点补上的是覆盖率还是多次片元着色？
2. 为什么每个图元每个像素只跑一次片元着色器，边缘却能变淡？
3. `glBlitFramebuffer` 在本 Demo 模式 2 里，读、写分别绑的是哪个 FBO？
4. 为什么不能对 `GL_TEXTURE_2D_MULTISAMPLE` 直接 `texture()`？模式 3 多了哪一步？
5. （思考）若窗口创建时就 `GLFW_SAMPLES, 4`，还要不要离屏 MSAA FBO？和后处理怎么配合？

---

## 9. 阅读顺序与下一步

```
Day05 帧缓冲（FBO、blit）
  ↓
Day10 实例化
  ↓
Day11 抗锯齿（本文）← MSAA 覆盖率 + 离屏还原
  ↓
Part5 Day01 高级光照（Blinn-Phong）
```

---

## 10. 参考文献

1. LearnOpenGL CN — [抗锯齿](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/11%20Anti%20Aliasing/)
2. LearnOpenGL EN — [Anti Aliasing](https://learnopengl.com/Advanced-OpenGL/Anti-Aliasing)
3. 《Part4 Day05 — 帧缓冲》— 附件、完整性、全屏四边形
4. 本工程 `src/cppfile/main.cpp` — 4x 离屏 MSAA 与 blit 还原
