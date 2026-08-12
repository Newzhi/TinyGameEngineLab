# EBO 索引绘制与 Uniform 颜色（基于当前 `main.cpp`）

这份文档对应 Day 03 的 `Proj/src/main.cpp`。Day 02 你已经能用 **VBO + VAO + glDrawArrays** 画一个三角形，Day 03 的目标是：

1. 理解 **EBO（Element Buffer Object）** 是什么，为什么要用索引
2. 搞清楚 **`glDrawElements` 和 `glDrawArrays` 的区别**
3. 理解 **Uniform** 是什么，以及 **顶点颜色** 与 **Uniform 颜色** 两种上色方式
4. 知道当前代码里 **顶点属性颜色** 和 **Uniform API** 各自在做什么

---

## 1. Day 02 → Day 03：多了什么？

Day 02 的绘制链路：

```
清屏 → glUseProgram → glBindVertexArray → glDrawArrays → 交换缓冲
```

Day 03 在此基础上增加了：

| 新增内容 | 作用 |
|---------|------|
| **EBO** | 存顶点索引，让多个三角形复用同一批顶点 |
| **顶点颜色属性** | 每个顶点带 RGB，GPU 在三角形内部自动插值 |
| **Uniform API** | CPU 每帧向 Shader 传「全局常量」（如随时间变化的整体色调） |
| **glDrawElements** | 按 EBO 里的索引取顶点，替代顺序遍历 |

启动阶段多了一步：

```
glGenBuffers(EBO) → glBindBuffer(GL_ELEMENT_ARRAY_BUFFER) → glBufferData(上传 indices)
```

渲染循环里，`glDrawArrays` 被替换（或并存注释）为：

```
glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0)
```

---

## 2. 核心概念速览

| 概念 | 全称 | 一句话理解 |
|------|------|-----------|
| **EBO** | Element Buffer Object | 存「顶点编号」的 GPU 缓冲，告诉 GPU 按什么顺序拼三角形 |
| **索引绘制** | Indexed Drawing | 顶点只存一份，用索引引用，避免重复数据 |
| **顶点属性（Attribute）** | Vertex Attribute | 每个顶点各有一份的数据，如位置、颜色、UV |
| **Uniform** | 统一变量 | 一次 Draw Call 内所有顶点/片段共享的常量，由 CPU 设置 |
| **Varying** | 顶点与片段间变量 | 顶点着色器写出、片段着色器读入，GPU 会在图元内部插值 |

可以把它想成：

- **VBO** = 顶点仓库（坐标 + 颜色都存在这里）
- **EBO** = 装配说明书（第 1 步取几号顶点、第 2 步取几号顶点……）
- **顶点属性颜色** = 每个角一种颜色，中间渐变
- **Uniform 颜色** = 整张贴纸同一个颜色，CPU 随时改

---

## 3. EBO 是什么？为什么要用？

### 3.1 问题：画矩形需要几个顶点？

一个矩形 = 2 个三角形 = 6 个顶点。如果不用索引，你要么：

- 存 **6 个完整顶点**（4 个角被重复存储），浪费显存；要么
- 用 `glDrawArrays` 画两次三角形，代码啰嗦。

实际上矩形只有 **4 个不同的角点**。左上角那个点同时属于两个三角形，应该 **只存一次**。

### 3.2 解决方案：索引

只存 4 个顶点，再单独存一张「索引表」：

```cpp
// 4 个角点（每个顶点：位置 xyz + 颜色 rgb，共 6 个 float）
float vertices[] = {
     0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,   // 0：右上
     0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,   // 1：右下
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   // 2：左下
    -0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 0.0f    // 3：左上
};

// 6 个索引 → 拼成 2 个三角形
unsigned int indices[] = {
    0, 1, 3,   // 第一个三角形
    1, 2, 3    // 第二个三角形
};
```

索引 `0, 1, 3` 的意思是：「去 `vertices` 里取第 0、1、3 号顶点，组成一个三角形」。

> 当前 `main.cpp` 为了循序渐进，先用 **3 个顶点 + 3 个索引** 画单个彩色三角形；注释里保留了完整矩形的写法，取消注释并补全 4 个顶点即可画矩形。

### 3.3 EBO 在 OpenGL 里怎么表示？

EBO 本质上也是 **Buffer Object**，只是绑定目标不同：

| 对象 | 绑定目标 | 存什么 |
|------|---------|--------|
| VBO | `GL_ARRAY_BUFFER` | 顶点属性原始数据（float 数组） |
| EBO | `GL_ELEMENT_ARRAY_BUFFER` | 索引数据（unsigned int 数组） |

创建与上传流程（与 VBO 对称）：

```cpp
unsigned int EBO;
glGenBuffers(1, &EBO);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
```

### 3.4 EBO 和 VAO 的关系

**重要**：EBO 的绑定状态会被 **当前绑定的 VAO 记录**。

因此推荐顺序（与 Day 02 一致）：

```
glBindVertexArray(VAO);                          // 先绑 VAO
glBindBuffer(GL_ARRAY_BUFFER, VBO);              // 再绑 VBO、上传顶点
glBufferData(GL_ARRAY_BUFFER, ...);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);      // 再绑 EBO、上传索引
glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...);
glVertexAttribPointer(...);                      // 配置顶点属性
glBindVertexArray(0);                            // 可选：解绑
```

之后每帧只需 `glBindVertexArray(VAO)`，VBO 布局 **和** EBO 绑定会一起恢复。

### 3.5 数据示意图

```
┌──────────────────────────────────────────────────────────────┐
│  VBO（顶点仓库）                                               │
│  [顶点0: pos+color | 顶点1: pos+color | 顶点2: pos+color | …]  │
└────────────────────────────┬─────────────────────────────────┘
                             │ 索引「指向」顶点编号
                             ▼
┌──────────────────────────────────────────────────────────────┐
│  EBO（装配说明书）                                             │
│  [ 0, 1, 2,  1, 2, 3 ]  →  两个三角形，共享顶点 1、2          │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. glDrawArrays 与 glDrawElements 的区别

这是 Day 03 必须搞清的一对 API。

### 4.1 glDrawArrays：顺序取顶点

```cpp
glDrawArrays(GL_TRIANGLES, 0, 3);
//           图元类型      起始顶点号  顶点个数
```

含义：从 VBO 里 **按顺序** 取第 `0、1、2` 号顶点，拼成 1 个三角形。

- **不需要 EBO**
- 顶点在 VBO 里怎么排，GPU 就怎么读
- 适合：顶点不重复、结构简单的图元（单个三角形）

### 4.2 glDrawElements：按索引取顶点

```cpp
glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
//             图元类型      索引个数  索引类型        索引偏移
```

含义：到 EBO 里读 **3 个索引**，每个索引再去 VBO 取对应顶点，拼成 1 个三角形。

- **必须** 事先绑定 EBO（通过 VAO 恢复）
- `3` = 本例索引数组长度（1 个三角形 × 3 个索引）
- `GL_UNSIGNED_INT` = 索引元素类型，与 `unsigned int indices[]` 对应
- 最后一个 `0` = 从 EBO 开头读（字节偏移，不是顶点号）

画完整矩形时：

```cpp
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);  // 6 个索引 = 2 个三角形
```

### 4.3 对比表

| | `glDrawArrays` | `glDrawElements` |
|---|----------------|------------------|
| 依赖 EBO | 否 | 是 |
| 顶点复用 | 不支持（重复存顶点） | 支持（索引指向同一顶点） |
| 关键参数 | `first`（起始顶点）、`count`（顶点数） | `count`（索引数）、`type`（索引类型） |
| 典型场景 | 单个三角形、调试 | 矩形、网格、模型 |
| 当前代码 | 已注释保留 | 正在使用 |

### 4.4 执行流程对比

**glDrawArrays：**

```
VBO: [V0][V1][V2]  ──顺序读取──→  三角形(V0,V1,V2)
```

**glDrawElements：**

```
EBO: [0,1,2]  ──查表──→  VBO 取 V0,V1,V2  ──→  三角形(V0,V1,V2)
EBO: [1,2,3]  ──查表──→  VBO 取 V1,V2,V3  ──→  三角形(V1,V2,V3)
                              ↑
                         V1、V2 只存一份
```

---

## 5. 颜色：顶点属性 vs Uniform

Day 02 把颜色 **写死** 在片段着色器里（`FragColor = vec4(1.0, 0.5, 0.2, 1.0)`）。Day 03 引入两种更灵活的方式。

### 5.1 方式一：顶点属性颜色（当前 Shader 正在使用）

#### CPU 侧：顶点数据里带上 RGB

```cpp
float vertices[] = {
    0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,   // 右下，红色
   -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,   // 左下，绿色
    0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f    // 顶部，蓝色
};
```

每个顶点 **6 个 float**：前 3 个是位置，后 3 个是颜色。

#### 配置两个顶点属性

```cpp
// location 0：位置，stride = 6 * sizeof(float)，offset = 0
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// location 1：颜色，stride 相同，offset 跳过前 3 个 float
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);
```

`stride = 6 * sizeof(float)` 表示「每过一个顶点，指针前进 6 个 float」——这是 **交错存储（interleaved）** 的常见写法。

#### Shader 侧：Attribute → Varying → 输出

顶点着色器：

```glsl
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;   // 传给片段着色器，GPU 会在三角形内部插值

void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
}
```

片段着色器：

```glsl
in vec3 ourColor;    // 插值后的颜色（三角形的角颜色不同，内部会渐变）

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
```

**效果**：三个角分别是红、绿、蓝，三角形内部呈现平滑渐变。这是 **Attribute + 插值**，不是 Uniform。

#### 数据流

```
VBO 里每个顶点的 aColor
        ↓  顶点着色器原样传出
   out ourColor（每个顶点一份）
        ↓  光栅化阶段自动插值
   in ourColor（每个像素一份，介于三角各角颜色之间）
        ↓
   FragColor
```

---

### 5.2 方式二：Uniform（当前渲染循环已写 API，需配合 Shader 修改）

#### Uniform 是什么？

**Uniform** 是 GLSL 里由 **CPU 设置、GPU 只读** 的全局变量：

- 对 **一次 `glDrawElements` / `glDrawArrays` 调用** 内的所有顶点、所有像素 **值相同**
- 典型用途：变换矩阵、全局色调、时间、纹理采样器、开关参数
- 与 Attribute 对比：Attribute **每个顶点不同**；Uniform **整次绘制相同**

| | Attribute | Uniform |
|---|-----------|---------|
| 谁设置 | VBO + `glVertexAttribPointer` | `glUniform*` 系列 API |
| 在 VS/FS 里 | `in`（仅 VS 输入） | `uniform` 关键字 |
| 每顶点是否不同 | 是 | 否 |
| 典型例子 | 位置、法线、顶点色 | MVP 矩阵、整体 tint 色 |

#### CPU 侧：查询位置 + 传值

当前 `main.cpp` 渲染循环里已有：

```cpp
glUseProgram(shaderProgram);

double timeValue = glfwGetTime();
float greenValue = static_cast<float>(sin(timeValue) / 2.0 + 0.5);

int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor");
glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f);
```

逐步解释：

| API | 作用 |
|-----|------|
| `glfwGetTime()` | 获取程序运行秒数，用于做动画 |
| `sin(time) / 2 + 0.5` | 把 sin 输出映射到 `[0, 1]`，得到周期性变化的 `greenValue` |
| `glGetUniformLocation(program, "ourColor")` | 查 Shader 里名为 `ourColor` 的 uniform 变量位置（location） |
| `glUniform4f(location, r, g, b, a)` | 往该 location 写入一个 `vec4` |

> **注意**：`glUniform*` 必须在 **`glUseProgram` 之后** 调用，否则不知道往哪个 Program 写。

> **深入**：Uniform 为何能把 CPU 变量送进 GPU、location 与 `Shader.h` 如何封装 → [Uniform 原理与实现机制](./Uniform原理与实现机制.md)。

#### 若要用 Uniform 控制整体颜色，Shader 应改成：

片段着色器示例（**Uniform 版本**，与当前 Attribute 版本二选一或组合使用）：

```glsl
#version 330 core
out vec4 FragColor;

uniform vec4 ourColor;   // CPU 每帧更新的全局颜色

void main()
{
    FragColor = ourColor;
}
```

此时顶点着色器 **不需要** `aColor` 和 `out ourColor`，VBO 也可以只存位置（stride 改回 `3 * sizeof(float)`）。

#### 当前代码的状态说明

当前 Shader 里 `ourColor` 是 **顶点着色器的 `out` / 片段着色器的 `in`（varying）**，**不是** `uniform`。因此：

- `glGetUniformLocation(..., "ourColor")` 会返回 **-1**（找不到 uniform）
- `glUniform4f(-1, ...)` 会被 **静默忽略**，不影响画面
- **实际看到的渐变彩色三角形**，来自 VBO 里的 **顶点属性颜色插值**

这是学习过程中的正常过渡：CPU 侧 Uniform API 已就位，下一步改 Shader 声明 `uniform vec4 ourColor` 即可看到「整三角形随时间变绿」的效果。

#### Uniform 常见 API

| API | 写入类型 |
|-----|---------|
| `glUniform1i(location, v)` | `int` |
| `glUniform1f(location, v)` | `float` |
| `glUniform2f / 3f / 4f` | `vec2 / vec3 / vec4` |
| `glUniformMatrix4fv` | `mat4`（后面学变换矩阵时用） |

命名规则：`glUniform` + **维度** + **类型后缀**（`i`/`f`/`fv` 等）。

---

### 5.3 两种方式可以组合

进阶写法：顶点属性提供「底色」，Uniform 提供「整体乘色 / 透明度」：

```glsl
// 片段着色器
in vec3 ourColor;
uniform vec4 tintColor;

void main()
{
    FragColor = vec4(ourColor, 1.0) * tintColor;
}
```

这样既能保留顶点渐变，又能用 CPU 每帧调整体色调。

---

## 6. Day 03 完整数据流（对照当前代码）

### 6.1 启动阶段（一次性）

```
createShaderProgram          // 顶点色 Attribute + varying 传递
glGenVertexArrays / VBO / EBO
glBindVertexArray(VAO)
  glBindBuffer(VBO) + glBufferData(vertices)     // 位置+颜色交错存储
  glBindBuffer(EBO) + glBufferData(indices)      // 索引
  glVertexAttribPointer(0, ...)                  // 位置 location 0
  glVertexAttribPointer(1, ...)                  // 颜色 location 1
glBindVertexArray(0)
```

### 6.2 渲染循环（每帧）

```
processInput
glClearColor + glClear
glUseProgram(shaderProgram)
  glGetUniformLocation + glUniform4f    // Uniform API（待 Shader 声明 uniform 后生效）
glBindVertexArray(VAO)
glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0)
glfwSwapBuffers + glfwPollEvents
```

### 6.3 从 CPU 到屏幕的完整链路

```
┌─────────────────────────────────────────────────────────────────────┐
│ CPU                                                                  │
│  vertices[]（位置+颜色）  indices[]（索引）                            │
│  glUniform4f（全局颜色，可选）                                         │
└────────────┬───────────────────────────────┬────────────────────────┘
             │ glBufferData                   │ glUniform*（→ Shader uniform）
             ▼                                ▼
┌────────────────────────┐         ┌─────────────────────────────────┐
│ GPU: VBO + EBO + VAO   │         │  Shader Program                  │
│  VAO 记住布局和 EBO 绑定 │ ──────→ │  VS: aPos, aColor → ourColor    │
└────────────┬───────────┘         │  FS: ourColor → FragColor        │
             │ glDrawElements       └─────────────────┬───────────────┘
             ▼                                        ▼
        按 EBO 取顶点 → 光栅化 → 插值颜色 → 帧缓冲 → 屏幕
```

---

## 7. 常用 API 速查（Day 03 新增部分）

### 7.1 EBO

- `glGenBuffers(1, &EBO)` — 生成 buffer id（与 VBO 共用同一套 Gen API）
- `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)` — 绑定为索引缓冲
- `glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW)` — 上传索引
- `glDeleteBuffers(1, &EBO)` — 释放（与 VBO 相同 API）

### 7.2 绘制

- `glDrawElements(mode, count, type, indicesOffset)`
  - `mode`：`GL_TRIANGLES`、`GL_LINES` 等
  - `count`：**索引个数**（不是顶点个数）
  - `type`：`GL_UNSIGNED_INT` / `GL_UNSIGNED_SHORT` / `GL_UNSIGNED_BYTE`
  - `indicesOffset`：EBO 内的字节偏移，通常 `(void*)0`

### 7.3 Uniform

- `glGetUniformLocation(program, "name")` — 查 uniform 变量 location，找不到返回 -1
- `glUniform4f(location, x, y, z, w)` — 设置 `vec4` / `rgba`
- 调用前必须 `glUseProgram(program)`

### 7.4 顶点属性（Day 03 扩展）

- 多个 `glVertexAttribPointer` + `glEnableVertexAttribArray` 对应多个 `layout(location = N) in ...`
- 交错存储时 **stride 相同**，**offset 不同**

---

## 8. 常见问题

| 现象 | 可能原因 |
|------|----------|
| 什么都画不出来 | 忘了绑 VAO；或 EBO 在 VAO 外绑定，导致 VAO 没记住 EBO |
| 图形错乱、像随机三角形 | `indices` 写错；或 `glVertexAttribPointer` 的 stride/offset 与顶点布局不一致 |
| `glDrawElements` 无效 | 仍用 `glDrawArrays` 的参数思维（`count` 应是索引数）；或未上传 EBO |
| Uniform 设置了但颜色不变 | Shader 里没有 `uniform` 声明；或 `glGetUniformLocation` 返回 -1；或忘了 `glUseProgram` |
| 三角形是纯色的，没有渐变 | 用了 Uniform 整体色，而不是顶点 Attribute 色；或三个顶点颜色设成相同 |
| 只有背景色 | 忘了 `glDrawElements`；索引 `count` 为 0 |
| 注释里矩形画不出来 | 只 uncomment `indices` 第二行不够，还需把 `vertices` 补全为 4 个顶点，并把 `glDrawElements` 的 count 改为 6 |

---

## 9. 和 Day 02 的对比

| | Day 02 | Day 03 |
|---|--------|--------|
| 顶点数据 | 仅位置（3 float/顶点） | 位置 + 颜色（6 float/顶点，交错存储） |
| 缓冲对象 | VBO + VAO | VBO + **EBO** + VAO |
| Draw Call | `glDrawArrays(GL_TRIANGLES, 0, 3)` | `glDrawElements(GL_TRIANGLES, 3, ...)` |
| 上色方式 | 片段着色器写死橙色 | **顶点属性插值** + Uniform API（待 Shader 对齐） |
| Shader 输入 | 仅 `aPos` | `aPos` + `aColor`；varying `ourColor` |
| 理解重点 | 数据怎么进 GPU | **索引复用顶点**、**Attribute vs Uniform**、两种 Draw API |

---

## 10. 动手练习建议

1. **画完整矩形**：补全 4 顶点 + 6 索引，改 `glDrawElements` 的 count 为 6，观察 EBO 复用效果。
2. **切回 glDrawArrays**：注释 `glDrawElements`，取消注释 `glDrawArrays`，对比两者差异。
3. **启用 Uniform 颜色**：把片段着色器改成 `uniform vec4 ourColor`，去掉顶点色 Attribute，看三角形整体随 `sin(time)` 变绿。
4. **Attribute + Uniform 组合**：保留顶点渐变，再乘一个 Uniform tint，理解两种变量的分工。
5. **改索引顺序**：把 `0, 1, 2` 改成 `0, 2, 1`，观察三角形绕序（后面学背面剔除时会用到）。

完成以上练习，你就掌握了「索引网格 + CPU 传参着色」的基础，为 Day 04 的 **变换矩阵（uniform mat4）** 打下基础。
