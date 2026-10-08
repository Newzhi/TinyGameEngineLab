# 几何着色器 Geometry Shader（Day 09）

对应 LearnOpenGL「[几何着色器](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/09%20Geometry%20Shader/)」：在顶点着色器和片元着色器之间可以再插一个可选阶段。它的输入是**一个图元的全部顶点**，可以改这些顶点，也可以发射出更多顶点、换成另一种图元。

| 文档 | 内容 |
|------|------|
| [Day08 高级 GLSL](../Day08/高级GLSLAdvancedGLSL.md) | 接口块；几何着色器的输入就是这种块组成的数组 |
| **本文档** | `layout` 修饰符、直通点 Demo、`gl_in`、房子、爆破、法线可视化 |
| 后续 | 实例化、抗锯齿 |

**工程对照**：当前 `main.cpp` 是立方体几何着色器 Demo。`1` 完整立方体，`2` 六个面沿面法线分开再合上（爆破，默认），`3` 叠加黄色顶点法线。爆破在世界空间做，投影放在几何着色器里乘。

**前置**：顶点着色器写出 `gl_Position`、图元类型（点 / 线 / 三角形）、Day08 接口块、法线矩阵（观察空间里显示法线时用到）。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Geometry shader | **几何着色器** | 可选阶段。一次拿到一个图元的所有顶点，再决定输出什么图元 |
| Primitive | **图元** | 点、线或三角形。几何着色器按「整个图元」运行，不是按单个顶点 |
| Input layout | **输入布局** | `layout (points/lines/triangles) in`，必须和 `glDraw*` 的图元类型一致 |
| Output layout | **输出布局** | `layout (points/line_strip/triangle_strip, max_vertices = N) out` |
| `max_vertices` | **最大顶点数** | 这一次调用最多 `EmitVertex` 多少次。超出的顶点会被丢掉 |
| `gl_in` | **输入顶点数组** | 内建接口块数组，`gl_in[i].gl_Position` 是上个阶段写出的位置 |
| `EmitVertex` | **发射顶点** | 把当前的 `gl_Position`（以及你写出的 `out`）收进正在组装的图元 |
| `EndPrimitive` | **结束图元** | 把已经发射的顶点合成输出布局指定的那种图元 |
| Triangle strip | **三角形带** | 前 3 个顶点一个三角形，之后每多 1 个顶点再多一个三角形 |
| Pass-through | **直通** | 原样发射输入顶点，用来确认几何着色器已经链上 |
| Explode | **爆破** | 把每个三角形沿法线方向推出再拉回 |
| Adjacency | **邻接图元** | `lines_adjacency` / `triangles_adjacency`，额外带上相邻顶点。本章例子不用 |

---

## 1. 它在管线的哪一层

没有几何着色器时：

```
顶点着色器（每个顶点） → 图元组装 → 光栅化 → 片元着色器
```

有几何着色器时，它接在图元组装之后、光栅化之前：

```
顶点着色器 → 图元组装 → 几何着色器（每个图元一次） → 光栅化 → 片元着色器
```

一次调用能看到这个图元的全部顶点，所以可以：

- 原样放行（直通）
- 把一个点展开成一条线，或一座房子
- 把一个三角形沿法线挪开（爆破）
- 沿每个顶点的法线额外长出一条线段（法线可视化）

输出进光栅化之后，片元着色器只看见最终那些三角形或线段，不知道它们是几何着色器临时造出来的。

---

## 2. `layout` 是什么

`layout (...)` 是 GLSL 的**布局修饰符**：写在 `in` / `out` / `uniform` 前面，告诉驱动这块数据按什么规则进出，**不参与计算**。同一关键字在不同阶段含义不同。

| 写法 | 出现位置 | 告诉驱动什么 |
|------|----------|----------------|
| `layout (location = 0) in vec2 aPos` | 顶点着色器 | 这个属性走 VAO 的 0 号槽，对上 `glVertexAttribPointer(0, …)` |
| `layout (std140) uniform Matrices { … }` | 任意阶段 | Uniform 块按 std140 对齐，CPU 才能按偏移 `glBufferSubData` |
| `layout (binding = 0)` | OpenGL 4.2+ | Uniform 块挂到绑定点 0。本工程 3.3 用 `glUniformBlockBinding` |
| `layout (points) in` | **几何着色器** | 每次调用吃进一个点图元，`gl_in` 长度为 1 |
| `layout (points, max_vertices = 1) out` | **几何着色器** | 输出仍是点；这次调用最多 `EmitVertex` 1 次 |

没有几何着色器时，图元类型只由 `glDrawArrays` 的第一个参数决定。有了几何着色器，驱动还要知道：**进来的图元是哪种、出去的图元是哪种**。这两行 `layout` 就是这份说明书。少写任意一行，着色器编译会失败。

### 2.1 输入 `layout (… ) in`

必须和 CPU 绘制模式一致。括号里的数字是这个图元至少几个顶点，也是 `gl_in` 的有效长度：

| 布局 | 对应的绘制 | `gl_in` 长度 |
|------|------------|----------------|
| `points` | `GL_POINTS` | 1 |
| `lines` | `GL_LINES` 或 `GL_LINE_STRIP` | 2 |
| `triangles` | `GL_TRIANGLES` / `STRIP` / `FAN` | 3 |
| `lines_adjacency` | 带邻接的线 | 4 |
| `triangles_adjacency` | 带邻接的三角形 | 6 |

Demo 写 `layout (points) in`，所以 `main.cpp` 必须 `glDrawArrays(GL_POINTS, 0, 4)`。改成 `GL_TRIANGLES`，几何着色器收不到图元，四个绿点消失。

### 2.2 输出 `layout (… , max_vertices = N) out`

输出只能是三种：`points`、`line_strip`、`triangle_strip`。想要一个三角形，就输出 `triangle_strip` 并发射 3 个顶点。

`max_vertices` 是**这一次几何着色器调用**最多发射几次。不是整屏顶点上限。直通 Demo 每个点只发 1 个，写成 1。房子例子要发 5 个，写成 5。超出的 `EmitVertex` 会被丢掉，不会报错。

输入和输出可以不同：`points` 进、`line_strip` 出，就是「提交 4 个点，GPU 上变成 4 条线」。VBO 里仍然只有 4 个顶点。

### 2.3 和 `location` 的差别

`layout (location = 0)` 说的是**一个顶点里第几号属性**。  
几何着色器的 `layout (points) in` 说的是**一次调用吃进哪种图元**。  
两者都叫 layout，管的层不一样：一个对 VAO 槽位，一个对 `glDraw*` 的图元类型。

---

## 3. 输入数组与发射

上个阶段的位置在内建数组 `gl_in` 里。它是接口块，所以即便只有一个点，下标也要写 `[0]`：

```glsl
in gl_PerVertex
{
    vec4  gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];
```

这段不用自己声明，GLSL 已经提供 `gl_in`。

```glsl
void main()
{
    gl_Position = gl_in[0].gl_Position + vec4(-0.1, 0.0, 0.0, 0.0);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4( 0.1, 0.0, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}
```

`EmitVertex` 把**当时**的 `gl_Position` 收进去。`EndPrimitive` 把已发射的顶点收成一条线。一次几何着色器调用里可以多次 `EndPrimitive`，从而从一个输入图元造出多个输出图元。

所以上面这个着色器配 `glDrawArrays(GL_POINTS, 0, 4)` 时，四个点会变成四条短横线。绘制函数提交的仍是点，线段是 GPU 上临时生成的。

直通版本用来确认阶段已经链上。画面应和没加几何着色器时一样：

```glsl
layout (points) in;
layout (points, max_vertices = 1) out;

void main()
{
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    EndPrimitive();
}
```

编译类型是 `GL_GEOMETRY_SHADER`，和顶点、片元一样要检查编译和链接错误，然后 `glAttachShader` 再 `glLinkProgram`。

---

## 4. 造房子

教程用四个 NDC 里的点当输入。顶点着色器只把 `vec2` 写成 `gl_Position`，不乘矩阵。几何着色器把每个点展开成 5 个顶点的三角形带：底边两个、顶边两个、再加一个屋顶。5 个顶点得到 `5 - 2 = 3` 个三角形（方盒子两个，屋顶一个）。

```glsl
#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 5) out;

void build_house(vec4 position)
{
    gl_Position = position + vec4(-0.2, -0.2, 0.0, 0.0); // 左下
    EmitVertex();
    gl_Position = position + vec4( 0.2, -0.2, 0.0, 0.0); // 右下
    EmitVertex();
    gl_Position = position + vec4(-0.2,  0.2, 0.0, 0.0); // 左上
    EmitVertex();
    gl_Position = position + vec4( 0.2,  0.2, 0.0, 0.0); // 右上
    EmitVertex();
    gl_Position = position + vec4( 0.0,  0.4, 0.0, 0.0); // 屋顶
    EmitVertex();
    EndPrimitive();
}

void main()
{
    build_house(gl_in[0].gl_Position);
}
```

顶点顺序不能随便换。三角形带是「第 1、2、3 个顶点」「第 2、3、4 个」「第 3、4、5 个」。先写完底边再写屋顶，房子才会闭合成教程那张图。`max_vertices` 必须 ≥ 5，否则屋顶被丢掉。

### 3.1 把颜色送过几何着色器

给每个点加一个 `vec3` 颜色。顶点着色器用 Day08 的接口块送出去：

```glsl
out VS_OUT {
    vec3 color;
} vs_out;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    vs_out.color = aColor;
}
```

几何着色器里**同名块必须是数组**，因为输入是整个图元。点图元也要写成 `gs_in[0]`：

```glsl
in VS_OUT {
    vec3 color;
} gs_in[];

out vec3 fColor;
```

发给片元着色器的 `fColor` 不是数组。每个 `EmitVertex` 会带走**当时**写在 `fColor` 里的值。房子四个墙顶点用 `gs_in[0].color`，屋顶发射前改成白色，屋顶就是雪：

```glsl
fColor = gs_in[0].color;
// ... 发射左下、右下、左上、右上 ...
gl_Position = position + vec4(0.0, 0.4, 0.0, 0.0);
fColor = vec3(1.0, 1.0, 1.0);
EmitVertex();
EndPrimitive();
```

不用接口块也可以写 `in vec3 vColor[]`，只要顶点着色器的 `out vec3 vColor` 同名。接口块在顶点很多时更好管理。

这些房子的顶点不在 VBO 里，是绘制那四个点时在 GPU 上生成的。重复的小形状（草、体素方块）适合这样做。形状很大或每栋都不同，仍应放在顶点缓冲里。

---

## 5. 爆破

对模型的每个三角形算一个法线，再沿法线把三个顶点推出去。`sin(time)` 从 -1 收到 1，教程把它映射到 `[0, 1]`，所以三角形只会往外走，再回到原位，不会穿进模型里面。

```glsl
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

vec3 GetNormal()
{
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 n = normalize(cross(a, b));
    if (dot(n, gs_in[0].normal) < 0.0)
        n = -n;   // 保证往外推
    return n;
}

vec4 explode(vec4 position, vec3 normal)
{
    float magnitude = 2.0;
    vec3 direction = normal * ((sin(time) + 1.0) / 2.0) * magnitude;
    return position + vec4(direction, 0.0);
}
```

`cross(a, b)` 的顺序决定法线朝哪边。对调 `a` 和 `b`，三角形会朝反方向飞。

发射时要把该顶点自己的纹理坐标一并写出，片元着色器才能采样到原来的贴图：

```glsl
gl_Position = explode(gl_in[0].gl_Position, normal);
TexCoords = gs_in[0].texCoords;
EmitVertex();
// 顶点 1、2 同样处理
EndPrimitive();
```

CPU 每帧 `setFloat("time", glfwGetTime())`。

这里的法线是用 `gl_Position` 上的三条边叉乘出来的。本工程把位置停在**世界空间**再叉乘，投影放在 `EmitVertex` 之前乘，六个面会沿真实朝外的方向分开。顶点着色器若已经乘了 `projection`，叉乘发生在裁剪空间，透视会把方向拉歪。

立方体同一面的两个三角形共面，叉乘法线平行，所以看起来是 **6 个面**在分离，不是 12 个三角形各飞各的。叉乘顺序若和环绕相反，面会往里收：Demo 里用顶点法线做一次点乘，朝里就取反。

CPU 每帧 `setFloat("time", glfwGetTime())`。要看法线是否可信，用下一节的可视化，不要用这个叉乘结果当光照法线。

---

## 6. 法线可视化

画两遍：第一遍正常着色；第二遍换一套带几何着色器的程序，只画法线。

```cpp
shader.use();
DrawScene();
normalDisplayShader.use();
DrawScene();
```

顶点着色器把位置变到**观察空间**就停，投影留给几何着色器。法线乘法线矩阵（`transpose(inverse(view * model))` 的 3×3），和位置待在同一个空间：

```glsl
gl_Position = view * model * vec4(aPos, 1.0);
mat3 normalMatrix = mat3(transpose(inverse(view * model)));
vs_out.normal = normalize(normalMatrix * aNormal);
```

几何着色器对三角形的三个顶点各发一条线段。`max_vertices = 6`，因为 3 条线 × 2 个端点，而且每条线自己 `EndPrimitive`：

```glsl
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

void GenerateLine(int index)
{
    gl_Position = projection * gl_in[index].gl_Position;
    EmitVertex();
    gl_Position = projection * (gl_in[index].gl_Position
                  + vec4(gs_in[index].normal, 0.0) * MAGNITUDE);
    EmitVertex();
    EndPrimitive();
}
```

`MAGNITUDE`（教程用 `0.4`）只缩放画出来的线段长度，不改模型上的法线数据。片元着色器把这些线涂成单一颜色（教程是黄色）。

线朝外且长度一致，说明顶点法线、属性 location、法线矩阵这三处是对的。光照发黑或发灰时，先看这些线再改光照公式。

---

## 7. 和本工程怎么接

### 7.1 Demo 内容（当前 `main.cpp`）

一个略微转过的立方体。数字键切换：

| 键 | 画面 | 着色器 |
|----|------|--------|
| `1` | 完整立方体，漫反射 | `gsCube.vs` / `gsCube.fs`，无几何着色器 |
| `2` | 六个面沿面法线推开再合上（默认） | `gsExplode.vs` + `gsExplode.gs` + `gsCube.fs` |
| `3` | 立方体 + 黄色顶点法线 | 先 `gsCube`，再 `gsNormal.vs` / `.gs` / `.fs` 画第二遍 |

两套几何着色器的输入都是 `layout (triangles) in`，对上 `glDrawArrays(GL_TRIANGLES, 0, 36)`。

**面分离（模式 2）**

1. 顶点着色器只乘 `model`，`gl_Position` 停在世界空间，法线经法线矩阵写入接口块。
2. 几何着色器对每个三角形：三条边叉乘得面法线，和顶点法线点乘，朝里则取反。
3. `sin(time)` 映射到 `[0,1]`，沿面法线平移三个顶点，再乘 `projection * view` 后发射。
4. 同一面两个三角形共面，位移相同，看起来是整面离开立方体。

**法线外显（模式 3）**

1. 先按模式 1 画立方体。
2. 同一 VAO 再画一遍：顶点着色器把位置和法线变到观察空间。
3. 几何着色器对三个顶点各 `EmitVertex` 两次，输出 `line_strip`，`max_vertices = 6`。`MAGNITUDE = 0.35` 只影响线段长度。

`geometryPass.*` 仍是直通点的最小例子，当前 `main.cpp` 没有挂它。`Shader` 第三参数继续用来链 `GL_GEOMETRY_SHADER`。

---

## 8. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 几何着色器编译失败，提示 layout | 少了 `in` 或 `out` 那一行 layout；或 `max_vertices` 没写 |
| 画面和没写几何着色器时一样，但也没有报错 | 直通着色器就是这样。模式 2 应看到面在动 |
| 六个面往里收 / 穿插 | 叉乘顺序和环绕相反。应对顶点法线做点乘，朝里取反 |
| 面分离方向被拉歪、近大远小 | 顶点着色器里已经乘了 `projection`。应停在世界 / 观察空间 |
| 法线线段和物体对不上 | 位置和法线不在同一空间；可视化应在观察空间算完再乘投影 |
| 什么都看不见 | 输入布局和绘制模式不一致，例如着色器写 `points` 却 `glDrawArrays(GL_TRIANGLES, ...)`；或只 `EmitVertex` 不 `EndPrimitive` |
| 房子缺屋顶或少一条边 | `EmitVertex` 次数超过 `max_vertices`，多出来的被丢弃 |
| 房子形状散成别的三角形 | 三角形带的顶点顺序错了。带是重叠的三个顶点一组，不是每三个顶点一个独立三角形 |
| 颜色整栋房子都是黑的 | 几何着色器里接口块没写成数组，或块名和顶点着色器不一致 |
| 屋顶不是白色 | `fColor = vec3(1.0)` 写在了屋顶那次 `EmitVertex` 之后。发射带走的是调用当时的值 |
| 爆破方向反了 | `cross(a, b)` 的 `a`、`b` 对调了 |
| 法线和物体不跟手、近大远小被拉歪 | 位置已经乘了 `projection` 才进几何着色器。可视化应在观察空间算完，再乘投影 |
| 法线长度夸张 | `MAGNITUDE` 太大。它只影响线段显示长度 |

---

## 9. 小练习

1. `layout (points) in` 时，`gl_in` 里有几个顶点？改成 `triangles` 之后呢？`layout (location = 0)` 管的是同一件事吗？
2. `EmitVertex` 和 `EndPrimitive` 各做一件什么事？只 `EmitVertex` 两次、从不 `EndPrimitive`，线段还会出现吗？
3. 5 个顶点的三角形带会产生几个三角形？顶点顺序为什么是「左下、右下、左上、右上、屋顶」？
4. 为什么几何着色器里的 `in VS_OUT { ... } gs_in[]` 必须是数组，即便点图元只有一个顶点？
5. 法线可视化的顶点着色器为什么不乘 `projection`？投影乘在几何着色器的哪一行？
6. （思考）`max_vertices = 6` 画三条法线刚好用完。若改成 5，第三条线会怎样？

---

## 10. 阅读顺序与下一步

```
Day08 高级 GLSL（接口块）
  ↓
Day09 几何着色器（本文）← 一个图元进，可以多个图元出
  ↓
实例化（一次绘制很多份；gl_InstanceID）
  ↓
抗锯齿
```

---

## 11. 参考文献

1. LearnOpenGL CN — [几何着色器](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/09%20Geometry%20Shader/)
2. LearnOpenGL EN — [Geometry Shader](https://learnopengl.com/Advanced-OpenGL/Geometry-Shader)
3. 《Part4 Day08 — 高级 GLSL》— 接口块；几何着色器的输入是这块的数组
4. 本工程 `src/cppfile/main.cpp` — 立方体面分离 + 法线外显；`shaders/gsExplode.gs`、`shaders/gsNormal.gs`
