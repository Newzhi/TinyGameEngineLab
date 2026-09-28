# 高级 GLSL Advanced GLSL（Day 08）

对应 LearnOpenGL「[高级 GLSL](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/08%20Advanced%20GLSL/)」：着色器里以 `gl_` 开头的内建变量、把阶段之间的变量打成**接口块**，以及让多个着色器程序共用同一份 uniform 的 **Uniform 缓冲对象**。

| 文档 | 内容 |
|------|------|
| [Day07 高级数据](../Day07/高级数据AdvancedData.md) | `glBufferData` / `glBufferSubData`；UBO 就是再把缓冲绑到另一个目标 |
| **本文档** | `gl_PointSize`、`gl_VertexID`、`gl_FragCoord`、`gl_FrontFacing`、`gl_FragDepth`、接口块、UBO / `std140` |
| [Day09 几何着色器](../Day09/几何着色器GeometryShader.md) | 图元展开、`EmitVertex`；接口块在这里变成数组 |

**工程对照**：当前 `main.cpp` 是 Uniform 缓冲 Demo。四个立方体各用一套程序（红 / 绿 / 蓝 / 黄）。`projection` 和 `view` 放在同一块 UBO 里，每帧用两次 `glBufferSubData` 写给全部四个程序；每个立方体只 `setMat4("model")`。内建变量和接口块是本章的知识点，这个 Demo 没有单独画它们。窗口是 **OpenGL 3.3 Core**：绑定点用 `glUniformBlockBinding`；`layout(binding = …)` 和 `layout(depth_greater)` 要 OpenGL 4.2。

**前置**：uniform、`gl_Position`、Day01 深度与 Early-Z、Day04 环绕与面剔除、Day07 先分配再 `glBufferSubData`。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Built-in variable | **内建变量** | GLSL 预定义、以 `gl_` 开头，不用自己声明就能读或写 |
| `gl_PointSize` | **点大小** | 顶点着色器输出，单位是像素；需先 `glEnable(GL_PROGRAM_POINT_SIZE)` |
| `gl_VertexID` | **顶点编号** | 当前顶点在这次绘制里的序号；`glDrawElements` 时是索引值 |
| `gl_FragCoord` | **片元窗口坐标** | `xy` 是窗口像素，原点在左下角；`z` 是深度 |
| `gl_FrontFacing` | **是否正面** | 当前片元来自正面则为 `true` |
| `gl_FragDepth` | **片元深度输出** | 在片元着色器里改写深度；一写就会关掉 Early-Z |
| Interface block | **接口块** | 用 `in` / `out` 把一组变量包成一块，块名在上下阶段必须一致 |
| Uniform block | **Uniform 块** | 着色器里声明的一组 uniform，数据放在缓冲对象中 |
| Uniform Buffer Object (UBO) | **Uniform 缓冲对象** | 绑到 `GL_UNIFORM_BUFFER` 的缓冲，可被多个着色器程序读取 |
| Binding point | **绑定点** | 数字插槽：一边挂 UBO，一边挂着色器里的 Uniform 块 |
| `std140` | **标准布局** | 按固定规则算每个成员的偏移，自己就能 `glBufferSubData` |
| Base alignment | **基准对齐量** | 该成员（含填充）占用的对齐单位 |
| Aligned offset | **对齐偏移量** | 从块开头算的字节偏移，必须是基准对齐量的倍数 |

---

## 1. 内建变量是什么

顶点属性、uniform、采样器都是你声明后传进去的。除此之外，GLSL 还提供一批 `gl_` 变量，直接反映管线状态。

前面已经用过两个：

| 变量 | 阶段 | 作用 |
|------|------|------|
| `gl_Position` | 顶点着色器，写出 | 裁剪空间位置。不写它，图元不会出现在屏幕上 |
| `gl_FragCoord` | 片元着色器，只读 | Day01 里用过它的 `z` 当深度 |

本章补顶点侧的点大小和顶点编号，以及片元侧的窗口坐标、正反面和可写深度。当前 Demo 的四个立方体没有用这些变量，颜色来自各自的片元着色器。

---

## 2. 顶点着色器变量

### 2.1 `gl_PointSize`

`GL_POINTS` 下每个顶点是一个点。点的像素大小可以：

- CPU：`glPointSize(size)`，这一次绘制里的点一样大
- 顶点着色器：写 `gl_PointSize`，每个顶点可以不同

着色器里改点大小默认是关的：

```cpp
glEnable(GL_PROGRAM_POINT_SIZE);
```

```glsl
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = gl_Position.z;
}
```

`gl_Position.z` 随远近变化，点就会近小远大或反过来，粒子常用这个。没开 `GL_PROGRAM_POINT_SIZE` 时，着色器里的赋值不起作用，仍由 `glPointSize` 决定。

### 2.2 `gl_VertexID`

只读整数，表示「现在正在处理哪个顶点」：

| 绘制函数 | `gl_VertexID` 是什么 |
|----------|----------------------|
| `glDrawArrays` | 从这次调用起点数起的顶点序号 |
| `glDrawElements` | 索引缓冲里当前这个顶点的索引 |

可以拿它做不用额外顶点属性的颜色、动画相位。它是输入，不能写。

---

## 3. 片元着色器变量

### 3.1 `gl_FragCoord`

`xy` 是窗口空间像素坐标，原点在窗口**左下角**。800×600 的窗口里，`x` 约在 0～800，`y` 约在 0～600。`z` 是该片元的深度（0～1），Day01 比的就是这个量。

用 `x` 可以把屏幕切成两半，左边一种结果、右边另一种，方便对比两种算法：

```glsl
void main()
{
    if (gl_FragCoord.x < 400.0)
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    else
        FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
```

`gl_FragCoord` 只读，改不了片元在窗口上的位置。

### 3.2 `gl_FrontFacing`

Day04 按环绕顺序区分正面和背面。面剔除关掉时，正反两面都会画出来，这个 `bool` 告诉你当前片元属于哪一面：正面为 `true`。

```glsl
if (gl_FrontFacing)
    FragColor = texture(frontTexture, TexCoords);
else
    FragColor = texture(backTexture, TexCoords);
```

箱子外面和里面就可以贴不同的图。`glEnable(GL_CULL_FACE)` 之后背面片元根本不会进来，这个判断就看不到背面那一支。

### 3.3 `gl_FragDepth`

深度可以在片元着色器里改写：

```glsl
gl_FragDepth = 0.0;   // 0.0～1.0，越小越近
```

没写它时，深度就是 `gl_FragCoord.z`。

一旦着色器里出现对 `gl_FragDepth` 的写，驱动没法在跑片元着色器之前知道深度，Day01 的 **Early-Z 会关掉**。这是这一行最大的代价。

OpenGL 4.2 起可以声明你只会把深度改到某一侧，硬件才有机会保留一部分提前测试：

```glsl
#version 420 core
layout (depth_greater) out float gl_FragDepth;

void main()
{
    FragColor = vec4(1.0);
    gl_FragDepth = gl_FragCoord.z + 0.1;
}
```

| 条件 | 含义 |
|------|------|
| `any` | 默认。深度任意改，Early-Z 关闭 |
| `greater` | 只会写成比 `gl_FragCoord.z` 更大（更远） |
| `less` | 只会写成更小（更近） |
| `unchanged` | 若写，只能写 `gl_FragCoord.z` 本身 |

本工程是 3.3，没有 `depth_greater` 这套声明。在 3.3 里写 `gl_FragDepth` 就等于放弃 Early-Z。

---

## 4. 接口块

变量一多，逐个 `in` / `out` 难对上。接口块把它们包在一起，写法接近结构体：

顶点着色器输出：

```glsl
out VS_OUT
{
    vec2 TexCoords;
} vs_out;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vs_out.TexCoords = aTexCoords;
}
```

片元着色器输入。**块名 `VS_OUT` 必须相同**，实例名可以不同（这里叫 `fs_in`，避免把输入块也叫成 `vs_out`）：

```glsl
in VS_OUT
{
    vec2 TexCoords;
} fs_in;

void main()
{
    FragColor = texture(tex, fs_in.TexCoords);
}
```

块名一致时，成员按声明顺序匹配。下一章几何着色器插在顶点和片元之间，接口块就是阶段之间的接口。

---

## 5. Uniform 缓冲对象

多个着色器都要 `projection` 和 `view` 时，现在的做法是每个程序各调用一次 `setMat4`。UBO 把这些共用的 uniform 放进**一块缓冲**，更新一次，所有链到同一绑定点的着色器都读到新值。每个物体仍不同的量（例如 `model`）继续用普通 uniform。

### 5.1 着色器里的 Uniform 块

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

块里的名字可以直接用，不必写 `Matrices.projection`。`layout (std140)` 指定下面这套内存规则。

### 5.2 为什么要 `std140`

缓冲只是字节，驱动不知道哪一段是 `view`。默认布局叫 **shared**：同一块在多个程序里偏移一致，但编译器可以为对齐挪动成员，CPU 侧事先不知道偏移，得用 `glGetUniformIndices` 查询。

**std140** 把每种类型的偏移规定死，手算即可，所有声明了同一块的程序布局相同。多占一些填充，换来不用查询。

**packed** 允许编译器删掉某个程序里用不到的成员，布局在程序之间可以不同，不适合手填。

常见规则（`N` = 4 字节）：

| 类型 | 基准对齐 |
|------|----------|
| `int` / `float` / `bool` | N（4 字节）。GLSL 的 `bool` 也是 4 字节 |
| `vec2` | 2N |
| `vec3` / `vec4` | 4N。`vec3` 也按 16 字节对齐 |
| 标量或向量的数组 | 每个元素都按 `vec4` 对齐（16 字节） |
| `mat4` | 4 个列向量，每列按 `vec4`，共 64 字节 |
| 结构体 | 成员按上面规则排完，再补齐到 16 的倍数 |

官网示例块的偏移：

```glsl
layout (std140) uniform ExampleBlock
{
                     // 基准对齐    对齐偏移
    float value;     // 4           0
    vec3  vector;    // 16          16   // 0+4 不是 16 的倍数，补到 16
    mat4  matrix;    // 16×4        32   // 四列：32, 48, 64, 80
    float values[3]; // 16          96   // 每个元素 16 字节，不是 4
                     //             112
                     //             128
    bool  boolean;   // 4           144
    int   integer;   // 4           148
};
```

`vec3` 后面、数组元素之间的填充是最容易写错的地方。`Matrices` 里两个 `mat4` 没有额外空洞：`projection` 在偏移 0，`view` 在偏移 64，整块 128 字节。`glm::mat4` 是列主序，和 GLSL 的 `mat4` 一致，`sizeof(glm::mat4)` 就是 64。

### 5.3 绑定点：缓冲和着色器怎么对上

上下文里有若干绑定点。UBO 挂到某个点，着色器里的 Uniform 块也挂到同一个点，两边就通了。

```
uboMatrices ──绑定点 0── Matrices 块（红 / 绿 / 蓝 / 黄四个程序）
uboLights   ──绑定点 2── Lights 块
```

本工程 3.3 的接法，每个程序都要做一次：

```cpp
unsigned int index = glGetUniformBlockIndex(shader.ID, "Matrices");
glUniformBlockBinding(shader.ID, index, 0);
```

OpenGL 4.2 起可以写在着色器里，省掉上面两行：

```glsl
layout(std140, binding = 0) uniform Matrices { ... };
```

UBO 一侧就是当前 `main.cpp` 的写法。`nullptr` 只预留 128 字节，每帧再填。观察矩阵每帧都改，所以用 `GL_DYNAMIC_DRAW`：

```cpp
const GLsizeiptr matricesSize = 2 * sizeof(glm::mat4);
glGenBuffers(1, &uboMatrices);
glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
glBufferData(GL_UNIFORM_BUFFER, matricesSize, nullptr, GL_DYNAMIC_DRAW);
glBindBuffer(GL_UNIFORM_BUFFER, 0);

glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, matricesSize);
```

`glBindBufferBase` 是「从 0 开始的整块对象」；`glBindBufferRange` 多一个偏移和大小，同一块 UBO 可以切成几段分给不同的块。Demo 的整段就是这两个矩阵，所以 Range 的偏移是 0、大小是 128 字节。

### 5.4 写入：还是 Day07 的 `glBufferSubData`

当前 Demo **每帧两段都写**。滚轮会改 `camera.Zoom`，拉窗口会改宽高比，投影不能只在进循环前写一次。视场和窗口大小都不变时，投影可以只写一次，只保留观察矩阵那一次 `glBufferSubData`。

```cpp
glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
glBindBuffer(GL_UNIFORM_BUFFER, 0);
```

这两次写入之后，红、绿、蓝、黄四个程序都不用再 `setMat4("view")` / `setMat4("projection")`。`model` 每个立方体不同，仍走普通 uniform。

改 `ExampleBlock` 里偏移 144 的 `bool` 时，GLSL 的 `bool` 是 4 字节，CPU 侧用 `int` 写入。Demo 的 `Matrices` 里没有这个成员，下面只是布局规则的例子：

```cpp
int b = 1;
glBufferSubData(GL_UNIFORM_BUFFER, 144, 4, &b);
```

### 5.5 换来什么

- 一组 uniform 一次写入，比每个程序各 `glUniform*` 一遍更省。
- 改一处，所有挂在该绑定点上的程序一起变。
- 单个着色器的普通 uniform 数量有上限（`GL_MAX_VERTEX_UNIFORM_COMPONENTS`）。骨骼矩阵这类大数组放进 UBO 后，上限按缓冲来算，余量更大。

---

## 6. 和本工程怎么接

### 6.1 画面

四个立方体各转了 25°，方便看出是立方体而不是色块。相机在 `(0, 0, 3)`。WASD、鼠标、滚轮、Esc 与前面几章相同。

| 位置 | 程序 | 颜色 |
|------|------|------|
| 左上 `(-0.75, 0.75, 0)` | `ubo.vs` + `uboRed.fs` | 红 |
| 右上 `(0.75, 0.75, 0)` | `ubo.vs` + `uboGreen.fs` | 绿 |
| 左下 `(-0.75, -0.75, 0)` | `ubo.vs` + `uboBlue.fs` | 蓝 |
| 右下 `(0.75, -0.75, 0)` | `ubo.vs` + `uboYellow.fs` | 黄 |

四个立方体共用一个 VAO，顶点只有位置，36 个，`glDrawArrays`。面剔除没有开。

### 6.2 初始化（对照 `main.cpp`）

1. 四次 `Shader(...)`，顶点着色器都是 `shaders/ubo.vs`，片元着色器各一份纯色。四套程序才能看出 UBO 是跨程序共享的；如果四个立方体共用一个程序，`view` 本来就只设一次，看不出差别。
2. `bindMatricesBlock` 对每个程序做 `glGetUniformBlockIndex(shader.ID, "Matrices")`，再 `glUniformBlockBinding(..., 0)`。块名必须和 `ubo.vs` 里的 `uniform Matrices` 一致，找不到会打印 `Matrices uniform block not found`。
3. `glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW)` 预留 128 字节，`glBindBufferRange(..., 0, uboMatrices, 0, 128)` 把整段挂到绑定点 0。
4. 立方体 VBO 用 `GL_STATIC_DRAW` 上传一次。顶点缓冲和 Uniform 缓冲是两块内存：一个给 `aPos`，一个给 `projection` / `view`。

### 6.3 每一帧

1. 用当前 `Zoom` 和窗口宽高算出 `projection`，用相机算出 `view`。
2. 绑定 `uboMatrices`，偏移 0 写入投影，偏移 64 写入观察矩阵，然后解绑。这一步不调用任何 `shader.use()`。
3. 绑立方体 VAO，循环四次：`use()` 对应程序，`translate` 到该角再 `rotate` 25°，`setMat4("model")`，`glDrawArrays(GL_TRIANGLES, 0, 36)`。

`Shader::setMat4` 仍是对**当前程序**的 `glUniformMatrix4fv`（`src/headfile/Shader.h`）。Demo 里它只传 `model`。

走动、滚轮缩放时四个立方体一起变，因为它们读的是同一段 UBO。改其中一个立方体的 `model` 不会带动另外三个。

相关文件：`src/cppfile/main.cpp`，`shaders/ubo.vs`，`shaders/uboRed.fs` / `uboGreen.fs` / `uboBlue.fs` / `uboYellow.fs`。

---

## 7. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 点的大小不变 | 没 `glEnable(GL_PROGRAM_POINT_SIZE)`，或画的不是 `GL_POINTS` |
| 左右分屏切在错误的位置 | `gl_FragCoord` 原点在左下角；窗口宽度不是 800 时，400 不是中线 |
| 箱子里面和外面颜色一样 | 开了面剔除，背面片元被丢掉，`gl_FrontFacing` 的 `else` 不会执行 |
| 写了 `gl_FragDepth` 后帧数掉下去 | Early-Z 被关掉。3.3 没有 `depth_greater` 可用来挽回 |
| 接口块对不上，成员是 0 | 上下阶段的**块名**不一致，或成员顺序、类型不一致。实例名不同是允许的 |
| 控制台 `Matrices uniform block not found` | 块名和 `ubo.vs` 里的 `uniform Matrices` 不一致，或该程序没链上这个顶点着色器 |
| 四个立方体透视一起错、相机不动 | `projection` / `view` 的偏移写反了。`std140` 下投影在 0，观察矩阵在 64 |
| 只有某一个颜色不跟相机走 | 那个程序漏了 `bindMatricesBlock`，它的 `Matrices` 没挂到绑定点 0 |
| UBO 更新了但物体全黑 | 块和缓冲没挂到同一个绑定点 |
| `vec3` 后面的 `float` 读错 | `std140` 里 `vec3` 对齐 16 字节，下一个标量不会紧挨在第 12 字节 |
| 数组 `float v[3]` 只占 12 字节 | 每个元素按 16 字节对齐，三个元素是 48 字节 |
| `bool` 写进去不对 | 用了 1 字节的 C++ `bool`。应按 4 字节整数写入 |
| `layout(binding = 0)` 编译失败 | 那是 OpenGL 4.2。本工程 3.3 用 `glUniformBlockBinding` |

---

## 8. 小练习

1. `gl_Position` 和 `gl_PointSize` 都是顶点着色器的输出。只写了 `gl_PointSize`、没开 `GL_PROGRAM_POINT_SIZE` 时，点有多大？
2. `glDrawArrays` 和 `glDrawElements` 里，`gl_VertexID` 各表示什么？
3. 为什么在片元着色器里写 `gl_FragDepth` 会让 Day01 的 Early-Z 失效？
4. 接口块的块名和实例名，哪一个必须在顶点着色器和片元着色器之间相同？
5. `std140` 下 `float` 后面紧跟 `vec3`，`vec3` 的对齐偏移为什么是 16 而不是 4？两个 `mat4` 的 `view` 从第几个字节开始？
6. （思考）四个着色器共用 `Matrices` 时，`model` 为什么不放进这块 UBO？

---

## 9. 阅读顺序与下一步

```
Day04 面剔除 → gl_FrontFacing 依赖同一套正反面
Day01 深度 / Early-Z → 写 gl_FragDepth 会关掉提前测试
Day07 高级数据 → UBO 的分配和 glBufferSubData
  ↓
Day08 高级 GLSL（本文）← 内建变量、接口块、Uniform 缓冲
  ↓
Day09 几何着色器
  ↓
实例化 …
```

---

## 10. 参考文献

1. LearnOpenGL CN — [高级 GLSL](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/08%20Advanced%20GLSL/)
2. LearnOpenGL EN — [Advanced GLSL](https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL)
3. 《Part4 Day01 — 深度测试》— `gl_FragCoord.z`、Early-Z
4. 《Part4 Day04 — 面剔除》— 环绕顺序与 `gl_FrontFacing`
5. 《Part4 Day07 — 高级数据》— `glBufferData(NULL)` 与 `glBufferSubData`
6. 本工程 `src/cppfile/main.cpp`、`shaders/ubo.vs` — 四套程序共用一块 `Matrices` UBO；`model` 仍走 `Shader::setMat4`
