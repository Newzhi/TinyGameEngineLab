# 混合 Blending（Day 03）

对应 LearnOpenGL「[混合](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/03%20Blending/)」：用 **alpha** 决定片段是「丢掉」还是「和背后颜色融在一起」，以及半透明物体为什么必须 **从远到近排序**。

| 文档 | 内容 |
|------|------|
| [Day01 深度测试](../Day01/深度测试DepthTesting.md) | 远近谁赢 |
| [Day02 模板测试](../Day02/模板测试StencilTesting.md) | 像素标签 / 遮罩 |
| **本文档** | alpha、`discard`、混合方程、`glBlendFunc`、透明物体排序 |
| [Day04 面剔除](../Day04/面剔除FaceCulling.md) | 环绕顺序、正/背面、为何片元不出现 |

**工程对照**：当前 `main.cpp` 为混合 Demo——不透明地板/箱子 + 草（丢弃）+ 半透明窗户（混合并按距离排序）。资源：

- `Resource/Texture/grass.png`
- `Resource/Texture/blending_transparent_window.png`
- `Resource/Texture/container2.png`（箱子）

**前置**：深度测试、纹理采样（RGBA）、每帧绘制顺序的直觉。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Blending | **混合** | 把当前片段颜色与颜色缓冲里已有颜色按公式合成 |
| Transparency | **透明度** | 能看见背后；由颜色第 4 分量 alpha 控制 |
| Alpha | **透明度分量** | `0` 全透，`1` 不透；`0.5` 约各占一半 |
| Discard | **丢弃片段** | 片元着色器里 `discard`，该像素不写颜色也不写深度 |
| Source color \(\bar{C}_{source}\) | **源颜色** | 当前要画上去的片段颜色 |
| Destination color \(\bar{C}_{destination}\) | **目标颜色** | 颜色缓冲里已经有的颜色 |
| Blend factor | **混合因子** | 源/目标各乘多少再加减 |
| `glBlendFunc` | **混合函数** | 指定源因子、目标因子 |
| `glBlendEquation` | **混合方程** | 源与目标是加还是减等（默认相加） |
| Order Independent Transparency (OIT) | **次序无关透明** | 高级技术；本文不展开，先掌握排序混合 |

---

## 1. 透明度是什么？

不透明物体：颜色就是自己。  
半透明物体（有色玻璃）：最终颜色 = **自己的颜色** + **背后已经画好的颜色**，按 alpha 加权。

颜色向量第四个分量：

| alpha | 观感 |
|-------|------|
| `1.0` | 完全不透明（教程此前一直这么设） |
| `0.5` | 约一半自己、一半背后 |
| `0.0` | 完全透明（若走混合，几乎只留背后） |

很多贴图除了 RGB 还有 **alpha 通道**。例如窗户贴图：玻璃区域 alpha≈0.25，四角 alpha=0。草贴图则几乎只有 `0` / `1`，没有中间值。

---

## 2. 两条路：丢弃 vs 真正混合

| | 丢弃 `discard` | 混合 Blending |
|--|----------------|---------------|
| 适用 | 草、铁丝网、树叶（非 0 即 1） | 玻璃、烟雾、半透明 UI |
| 中间透明度 | 不支持 | 支持 |
| 深度缓冲 | 透明像素不写入 → 少很多排序麻烦 | 半透明四边形仍会写深度 → **必须排序** |
| 性能 / 复杂度 | 简单 | 要开 `GL_BLEND` + 通常从远到近画 |

官网建议：**能丢弃就先丢弃**；必须半透明再上混合。

---

## 3. 加载带 Alpha 的纹理

`stb_image` 会按通道数加载；有 alpha 时 `nrChannels == 4`，上传要用 `GL_RGBA`：

```cpp
GLenum format = GL_RGB;
if (nrChannels == 1) format = GL_RED;
else if (nrChannels == 3) format = GL_RGB;
else if (nrChannels == 4) format = GL_RGBA;

glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
```

片元着色器取完整 `vec4`，不要强行丢 alpha：

```glsl
FragColor = texture(texture1, TexCoords); // 含 .a
```

### 3.1 Alpha 纹理的环绕方式

若仍用 `GL_REPEAT`，纹理边缘会和「对面边缘」做插值，容易在四边形周围出现半透明色边。  
对草 / 窗户这类 alpha 图，官网要求：

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

---

## 4. 丢弃片段（草）

草是贴在四边形上的 2D 图：只有草叶该看见，黑色/透明背景必须拿掉。

```glsl
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D texture1;

void main()
{
    vec4 texColor = texture(texture1, TexCoords);
    if (texColor.a < 0.1)
        discard;          // 后面不再写颜色 / 深度
    FragColor = texColor;
}
```

要点：

- 阈值 `0.1` 可按贴图微调，避免边缘毛刺  
- `discard` 后该片段**不更新深度**，所以背后物体仍能正确显示  
- 副作用：可能关掉 Early-Z 优化（FS 可能改「是否存在」），草很多时要注意 batch

本工程：多株草用同一 `grass.png` 四边形，不同 `translate`。

---

## 5. 混合方程（半透明窗户）

启用：

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

混合在 **片段着色器跑完、模板/深度测试都通过之后** 发生：

\[
\bar{C}_{result} = \bar{F}_{source} \cdot \bar{C}_{source} + \bar{F}_{destination} \cdot \bar{C}_{destination}
\]

常见「标准透明」：

- \(\bar{F}_{source} = \alpha_{source}\) → `GL_SRC_ALPHA`
- \(\bar{F}_{destination} = 1 - \alpha_{source}\) → `GL_ONE_MINUS_SRC_ALPHA`

例：绿色方块 alpha=0.6 画在已有红色上 → 约 60% 绿 + 40% 红。

### 5.1 常用因子（记常用即可）

| 枚举 | 含义 |
|------|------|
| `GL_ZERO` / `GL_ONE` | 0 / 1 |
| `GL_SRC_ALPHA` | 源的 alpha |
| `GL_ONE_MINUS_SRC_ALPHA` | \(1 - \) 源 alpha |
| `GL_DST_ALPHA` / `GL_ONE_MINUS_DST_ALPHA` | 目标 alpha 相关 |
| `GL_SRC_COLOR` / `GL_ONE_MINUS_SRC_COLOR` | 用颜色当因子 |

也可用 `glBlendFuncSeparate` 给 RGB 与 alpha 设不同因子；`glBlendEquation` 改成减/取 min/max（默认 `GL_FUNC_ADD` 够用）。

---

## 6. 为什么半透明必须排序？

深度缓冲**不管 alpha**：四边形只要深度测试通过，整块（含「看起来透明」的像素）都会写深度。

若先画近处窗户、再画远处窗户：

1. 近处窗户写入深度  
2. 远处窗户深度更大 → **深度测试失败被丢**  
3. 近处窗户的透明玻璃里看不到后面的窗户 → 穿帮

所以混合场景的绘制纪律：

```
1. 先画全部不透明物体（正常开深度写入）
2. 把透明物体按「到相机距离」从远到近排序
3. 再画透明物体（通常仍开深度测试，但常关掉深度写入 glDepthMask(FALSE)，
   避免透明面互相错误遮挡；本 Demo 按官网排序 + 默认深度写入也可先跑通）
```

草用 `discard` 时，透明像素不写深度，一般**不必**为草做这套排序。

### 6.1 简单排序（官网 map 写法）

```cpp
std::map<float, glm::vec3> sorted;
for (const glm::vec3& pos : windows)
{
    float distance = glm::length(camera.Position - pos);
    sorted[distance] = pos; // 注意：距离完全相同会覆盖，Demo 够用
}

// 从远到近：距离大的先画
for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
{
    // translate 到 it->second 再画窗户四边形
}
```

限制（文档里要心里有数）：

- 只按「物体中心点」距离，旋转/巨大网格/交叉透明面仍会错  
- 完整解法有 OIT 等，超出本章  

---

## 7. 和深度 / 模板怎么放在一起记？

片段路径（简化）：

```
FS 算出颜色（可含 alpha；可 discard）
        │
        ▼
模板测试 → 深度测试
        │
        ▼
若启用 GL_BLEND：与颜色缓冲混合后写入
否则：直接覆盖写入
```

| 技术 | 解决的问题 |
|------|------------|
| 深度 | 谁在前面 |
| 模板 | 自定义遮罩 / 特效 |
| 混合 | 半透明颜色怎么融 |

---

## 8. 和本工程怎么接

### 8.1 Demo 内容（当前 `main.cpp`）

对照官网 `4.advanced_opengl/3.1.blending_discard` 与 `3.2.blending_sorted`，两个示例合并在一个场景：

1. **不透明**：地板 + 两个箱子，先画完  
2. **草**（右半场景）：`grass.png` + `discard`，不开混合、不排序  
3. **窗户**（左半场景）：`blending_transparent_window.png`，`glEnable(GL_BLEND)` + `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`，按相机距离从远到近画  

相关文件：

| 文件 | 作用 |
|------|------|
| `src/cppfile/main.cpp` | 混合 Demo 主流程 |
| `shaders/blending.vs` | 四边形 / 箱子顶点 |
| `shaders/blending.fs` | 直接输出纹理（含 alpha） |
| `shaders/blendingDiscard.fs` | alpha 阈值丢弃 |
| `Resource/Texture/grass.png` | 草 |
| `Resource/Texture/blending_transparent_window.png` | 半透明窗 |

操作：`WASD` + 鼠标；绕着窗户转一圈，确认多层玻璃能互相透过。可临时去掉排序对比穿帮。

### 8.2 排错 checklist

| 现象 | 原因 |
|------|------|
| 草是一整块方片，`discard` 像没生效 | **贴图根本没加载成功**：`stbi_load` 失败时纹理里没有 alpha，条件永远不成立。先看控制台有没有 `Texture failed to load` |
| 贴图明明在源码目录却加载失败 | 资源没同步到构建目录。`file(COPY)` 只在 **CMake 配置期** 执行一次，之后新增的图不会被拷贝；本工程已改为 POST_BUILD `copy_directory` |
| 草四周一圈半透明色边 | alpha 贴图要用 `GL_CLAMP_TO_EDGE`，`GL_REPEAT` 会和对边插值 |
| 草上下颠倒 | 加载时是否翻转（`stbi_set_flip_vertically_on_load`）要和四边形 UV 的 y 方向配套。官网本章**不翻转**，所以 UV 的 y 写反 |
| 近处窗户把远处窗户整块挡住 | 没有按距离从远到近画，或不透明物体还没画完 |

> 注意：`Model.h` 里的 `TextureFromFile` 会把 `stbi_set_flip_vertically_on_load` 设成 `true`（配合 `aiProcess_FlipUVs`）。这是**全局状态**，所以本章的 `loadTexture` 每次都显式复位成 `false`。

---

## 9. 小练习

1. `discard` 和把 alpha 设为 0 再混合，对**深度缓冲**的影响有何不同？  
2. 为什么 `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` 对应「标准透明」？  
3. 为何透明物体要先画远的？结合深度写入说明。  
4. 草为什么通常不必排序，窗户却要？  
5. （动手）把窗户排序改成从近到远，截图对比。  
6. （思考）两片交错的半透明三角形，仅按中心距离排序为何不够？

---

## 10. 阅读顺序与下一步

```
Day01 深度测试
  ↓
Day02 模板测试
  ↓
Day03 混合（本文）← discard + BlendFunc + 排序
  ↓
Day04 面剔除 Face Culling → 帧缓冲 Framebuffers …
```

---

## 11. 参考文献

1. LearnOpenGL CN — [混合](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/03%20Blending/)  
2. LearnOpenGL EN — [Blending](https://learnopengl.com/Advanced-OpenGL/Blending)  
3. 《Part4 Day01 — 深度测试》— 深度写入与透明物体冲突的根源  
4. 本目录资源：`grass.png`、`blending_transparent_window.png`
