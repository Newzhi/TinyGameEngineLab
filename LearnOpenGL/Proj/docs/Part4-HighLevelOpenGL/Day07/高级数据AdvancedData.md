# 高级数据 Advanced Data（Day 07）

对应 LearnOpenGL「[高级数据](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/07%20Advanced%20Data/)」：缓冲对象本身只是一块内存；绑定到不同**缓冲目标**才决定它当顶点缓冲、索引缓冲还是复制用的中转。本章讲三种填数据的方式，以及**分批顶点属性**和**缓冲之间复制**。

| 文档 | 内容 |
|------|------|
| [Day06 立方体贴图](../Day06/立方体贴图Cubemaps.md) | 天空盒、环境映射 |
| **本文档** | `glBufferData` / `glBufferSubData` / `glMapBuffer`、分批属性、`glCopyBufferSubData` |
| [Day08 高级 GLSL](../Day08/高级GLSLAdvancedGLSL.md) | 内建变量、接口块；Uniform 缓冲会再用 `glBufferSubData` |

**工程对照**：当前 `main.cpp` 是缓冲 Demo。上面三块方块分别用整块上传、分批 `glBufferSubData`、映射后再复制；下面橙条每帧只改顶边的 Y。`Mesh.h` 仍是模型用的交错布局。

**前置**：VBO、VAO、`glVertexAttribPointer` 的 location / size / stride / offset。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Buffer object | **缓冲对象** | 一块由 OpenGL 管理的内存，本身没有「顶点」或「索引」的含义 |
| Buffer target | **缓冲目标** | 绑定时给这块内存派的角色，如 `GL_ARRAY_BUFFER` |
| `glBufferData` | **分配并整块填充** | 按 `size` 分配（或重新分配）GPU 内存，可选地把 CPU 数据拷进去 |
| Reserve | **预留** | `data` 传 `NULL`，只分配、不填充，之后再一点点写 |
| `glBufferSubData` | **局部更新** | 在已分配的区间里，从某个偏移开始覆盖一段 |
| `glMapBuffer` | **映射缓冲** | 拿到指向缓冲内存的指针，直接写，写完必须 `glUnmapBuffer` |
| Interleaved | **交错布局** | 每个顶点的位置、法线、UV 紧挨着：`PNTPNT…` |
| Batched attributes | **分批顶点属性** | 先全部位置，再全部法线，再全部 UV：`PPP…NNN…TT…` |
| Stride | **步长** | 从一个顶点的该属性，到下一个顶点同一属性的字节距离 |
| `glCopyBufferSubData` | **缓冲间复制** | 从读目标拷一段字节到写目标 |
| `GL_COPY_READ_BUFFER` / `GL_COPY_WRITE_BUFFER` | **复制专用目标** | 两个缓冲都想当 `GL_ARRAY_BUFFER` 时，用这两个目标同时绑定 |

---

## 1. 缓冲对象和缓冲目标

缓冲对象只管理一块内存。把它绑到某个目标，OpenGL 才按这个目标的规则使用它：

- 绑到 `GL_ARRAY_BUFFER` → 顶点属性数据（VBO）
- 绑到 `GL_ELEMENT_ARRAY_BUFFER` → 索引（EBO），且这个绑定记在**当前 VAO** 上

每个目标内部只记住「当前绑的是哪一个缓冲」。同一时刻，一个目标上只能有一个绑定。两个 VBO 不能同时绑到 `GL_ARRAY_BUFFER`，这是后面复制缓冲要用专用目标的原因。

到目前为止填充方式一直是一次 `glBufferData`：分配一块 GPU 内存，并把 CPU 数组整块拷过去。本章在此之外再加两种写法和一种缓冲间拷贝。

---

## 2. 三种填充方式

### 2.1 `glBufferData`：整块分配（本工程现状）

上一章天空盒 Demo 里的箱子就是这一路：每个顶点 8 个 float（位置 3 + 法线 3 + UV 2），交错存放。当前 Demo 的红方块同样是一次 `glBufferData`，只是属性改成位置 + 颜色，步长是 `sizeof(VertexPC)`。

```cpp
glBindVertexArray(cubeVAO);
glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
```

`Mesh.h` 同样是交错：整块 `Vertex` 数组一次上传，步长是 `sizeof(Vertex)`，偏移用 `offsetof`。

`data` 可以是 `NULL`。这时函数**只分配** `size` 字节，不拷贝内容。适合先按最大容量预留，之后用 `glBufferSubData` 或映射指针再填。

最后一个参数是使用提示，驱动据此决定把内存放哪：

| 提示 | 含义（直觉） |
|------|----------------|
| `GL_STATIC_DRAW` | 上传一次，GPU 反复读。三块方块用这个 |
| `GL_DYNAMIC_DRAW` | 会经常改，GPU 也经常读。下面那条橙条用这个 |
| `GL_STREAM_DRAW` | 几乎每帧改一次，用完就丢 |

提示不是硬性约束，写错也能画，只是可能更慢。

### 2.2 `glBufferSubData`：改已分配区间的一段

```cpp
glBufferSubData(GL_ARRAY_BUFFER, 24, sizeof(data), &data); // 覆盖 [24, 24 + sizeof(data))
```

| 参数 | 含义 |
|------|------|
| `target` | 当前绑着目标缓冲的那个目标 |
| `offset` | 从缓冲开头算的字节偏移 |
| `size` | 要写入的字节数 |
| `data` | CPU 侧源数据 |

和 `glBufferData` 的差别：它**不重新分配**，只覆盖已有区间。所以调用前必须已经 `glBufferData` 过，且 `offset + size` 不能超出已分配大小。适合「容量不变、只改中间一段」，以及下一节把位置 / 法线 / UV 分三段写入。

### 2.3 `glMapBuffer`：拿指针直接写

```cpp
glBindBuffer(GL_ARRAY_BUFFER, buffer);
void* ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
memcpy(ptr, data, sizeof(data));
glUnmapBuffer(GL_ARRAY_BUFFER);   // 成功返回 GL_TRUE，之后 ptr 不能再用
```

流程：

1. 绑定要写的缓冲（它必须已经由 `glBufferData` 分配过）。
2. `glMapBuffer` 返回可写指针。`GL_WRITE_ONLY` 表示这次只写、不读旧内容。
3. 用 `memcpy` 或自己的循环把数据写进去。
4. `glUnmapBuffer` 告诉驱动「写完了」。返回 `GL_TRUE` 才表示数据已经落到缓冲里。解除映射后指针失效。

适合不想先在 CPU 上拼一块临时数组的情况：例如从文件读出的顶点直接写入映射出来的内存。映射期间不要再对同一缓冲调用其它会碰这块内存的 GL 命令。

本工程窗口是 **OpenGL 3.3 Core**。`glMapBuffer` 在 Core 里已经去掉，Demo 用等价的 `glMapBufferRange`：偏移、长度、`GL_MAP_WRITE_BIT`，写完同样 `glUnmapBuffer`。

```cpp
void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof(data), GL_MAP_WRITE_BIT);
memcpy(ptr, data, sizeof(data));
glUnmapBuffer(GL_ARRAY_BUFFER);
```

---

## 3. 分批顶点属性

`glVertexAttribPointer` 描述的是「缓冲里的字节怎么切成属性」，不要求属性必须交错。

| 布局 | 内存顺序 | 本工程 |
|------|----------|--------|
| 交错 Interleaved | `位置,颜色, 位置,颜色, …` | Demo 红方块、蓝方块；`Mesh.h` 的 `sizeof(Vertex)` |
| 分批 Batched | `全部位置 … 全部颜色` | Demo 绿方块：`glBufferSubData` 分两段 |

从文件里经常先拿到三个独立数组。交错要把它们织成一条；分批可以原样分段写入：

```cpp
float positions[] = { /* ... */ };
float normals[]   = { /* ... */ };
float tex[]       = { /* ... */ };

// 先按总大小预留，data 为 NULL
glBufferData(GL_ARRAY_BUFFER,
             sizeof(positions) + sizeof(normals) + sizeof(tex),
             NULL, GL_STATIC_DRAW);

glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), &positions);
glBufferSubData(GL_ARRAY_BUFFER, sizeof(positions), sizeof(normals), &normals);
glBufferSubData(GL_ARRAY_BUFFER,
                sizeof(positions) + sizeof(normals), sizeof(tex), &tex);
```

属性指针要跟着改。下一步同类属性紧挨着，所以 **stride 等于这一个属性自己的大小**，offset 指到该段的开头：

```cpp
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                      3 * sizeof(float), (void*)(sizeof(positions)));
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                      2 * sizeof(float), (void*)(sizeof(positions) + sizeof(normals)));
```

对照红方块的交错指针：stride 是 `sizeof(VertexPC)`（6 个 float），颜色只是同一顶点内跳过 xyz。绿方块分批后 stride 变成 `3 * sizeof(float)`，颜色的 offset 是整段位置数组的字节长度。

两种都能画对。教程建议仍用**交错**：顶点着色器一次要用到的位置、法线、UV 在内存里挨着，缓存更友好。分批的好处是加载时少做一次织数据。

---

## 4. 复制缓冲

缓冲里已经有数据时，可以用 `glCopyBufferSubData` 拷到另一个缓冲，不必先读回 CPU。

```cpp
void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget,
                         GLintptr readOffset, GLintptr writeOffset,
                         GLsizeiptr size);
```

`readTarget` / `writeTarget` 是**当前绑定**着源缓冲和目标缓冲的那两个目标。例如把 VBO 拷进 EBO，就分别绑到 `GL_ARRAY_BUFFER` 和 `GL_ELEMENT_ARRAY_BUFFER`。

两个都是顶点缓冲时，不能同时绑到 `GL_ARRAY_BUFFER`。这时用专门的复制目标：

```cpp
glBindBuffer(GL_COPY_READ_BUFFER, vbo1);
glBindBuffer(GL_COPY_WRITE_BUFFER, vbo2);
glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                    0, 0, sizeof(vertexData));
```

读侧也可以继续用原来的 `GL_ARRAY_BUFFER`，只把写侧换到 `GL_COPY_WRITE_BUFFER`：

```cpp
glBindBuffer(GL_ARRAY_BUFFER, vbo1);
glBindBuffer(GL_COPY_WRITE_BUFFER, vbo2);
glCopyBufferSubData(GL_ARRAY_BUFFER, GL_COPY_WRITE_BUFFER,
                    0, 0, sizeof(vertexData));
```

拷贝的是字节，不关心里面是位置还是索引。`readOffset`、`writeOffset`、`size` 的单位都是字节，区间必须落在两边已经分配的范围内。

---

## 5. 和本工程怎么接

### 5.1 Demo 内容（当前 `main.cpp`）

启动后应看到三块同样大小的方块和下面一条会伸缩的橙条：

| 画面 | 缓冲里发生了什么 |
|------|------------------|
| 左，红 | `glBufferData` 一次写入交错的位置+颜色 |
| 中，绿 | `glBufferData(NULL)` 预留，两段 `glBufferSubData`：先位置、后颜色。属性 stride 是 `3 * sizeof(float)` |
| 右，蓝 | `glMapBufferRange` 写入 `srcVBO`，`glCopyBufferSubData` 拷到 `dstVBO`，VAO 绑的是拷贝结果 |
| 下，橙条 | 每帧对顶点 2、3、4 的 Y 各做一次 `glBufferSubData`（偏移 `(i * 6 + 1) * sizeof(float)`）。底边和颜色不被重写，所以只有顶边在动 |

相关文件：`src/cppfile/main.cpp`，`shaders/advancedData.vs` / `advancedData.fs`。

`Mesh.h` 的 `setupMesh` 仍是模型路径：整块 `Vertex` 交错上传，步长 `sizeof(Vertex)`，偏移用 `offsetof`。

下一章 Uniform 缓冲会把多个着色器共用的矩阵放进一块缓冲，并用 `glBufferSubData` 更新其中一段。本章的偏移、大小、先分配再写入，到那里直接用。

---

## 6. 容易踩的坑

| 现象 | 原因 |
|------|------|
| `glBufferSubData` 没效果或 GL 报错 | 还没 `glBufferData`，或 `offset + size` 超出已分配大小 |
| 改了映射指针但画面不变 | 忘了 `glUnmapBuffer`，或它返回了 `GL_FALSE` |
| 解除映射后崩溃 | 继续使用 `glMapBuffer` 返回的指针 |
| 分批后模型拉伸、法线乱 | stride 仍写成交错的 `8 * sizeof(float)`，或 offset 没用 `sizeof(positions)` 这类字节偏移 |
| 复制后目标是空的 / 报错 | 目标缓冲没先分配足够空间；或两个 VBO 都绑在 `GL_ARRAY_BUFFER` 上，后一次绑定把前一次挤掉了 |
| `sizeof(positions)` 不对 | `positions` 若是 `std::vector`，`sizeof` 是对象本身而不是元素总字节，应使用 `positions.size() * sizeof(float)` |

---

## 7. 小练习

1. `glBufferData` 的 `data` 传 `NULL` 时，缓冲里有没有你的顶点？接下来该调用哪个函数才能把数写进去？
2. 红方块的 stride 为什么是 `sizeof(VertexPC)`？绿方块 location 1 的 stride 和 offset 各是什么？
3. 为什么教程仍推荐交错，即使分批更好从文件直接写入？
4. 两个 VBO 都要参与复制时，为什么不能都绑到 `GL_ARRAY_BUFFER`？`GL_COPY_READ_BUFFER` 解决的是哪一件事？
5. （思考）`Mesh.h` 若改成「位置数组、法线数组、UV 数组分三段 `glBufferSubData`」，`glVertexAttribPointer` 的 stride 要不要改？EBO 那次 `glBufferData` 要不要改？

---

## 8. 阅读顺序与下一步

```
Day06 立方体贴图
  ↓
Day07 高级数据（本文）← 缓冲怎么分配、局部写、映射、复制
  ↓
Day08 高级 GLSL（Uniform Buffer 会用 glBufferSubData）
  ↓
几何着色器 / 实例化 …
```

---

## 9. 参考文献

1. LearnOpenGL CN — [高级数据](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/07%20Advanced%20Data/)
2. LearnOpenGL EN — [Advanced Data](https://learnopengl.com/Advanced-OpenGL/Advanced-Data)
3. 本工程 `src/cppfile/main.cpp` — 四种填缓冲方式的 Demo
4. 本工程 `src/headfile/Mesh.h` — `Vertex` 交错布局与 `offsetof`
