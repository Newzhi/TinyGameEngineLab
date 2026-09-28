# 几何着色器 Geometry Shader（Day 09）

对应 LearnOpenGL「[几何着色器](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/09%20Geometry%20Shader/)」：在顶点着色器和片元着色器之间可以再插一个可选阶段。它的输入是**一个图元的全部顶点**，可以改这些顶点，也可以发射出更多顶点、换成另一种图元。

| 文档 | 内容 |
|------|------|
| [Day08 高级 GLSL](../Day08/高级GLSLAdvancedGLSL.md) | 接口块；几何着色器的输入就是这种块组成的数组 |
| **本文档** | 输入/输出布局、`gl_in`、`EmitVertex` / `EndPrimitive`、房子、爆破、法线可视化 |
| 后续 | 实例化、抗锯齿 |

**工程对照**：当前 `main.cpp` 仍是 Day08 的四个立方体，没有几何着色器。`Shader` 的构造函数只编译 `GL_VERTEX_SHADER` 和 `GL_FRAGMENT_SHADER`（`src/headfile/Shader.h`）。要接上这一章，需要再 `glCreateShader(GL_GEOMETRY_SHADER)` 并在 `glLinkProgram` 之前 `glAttachShader`。

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

## 2. 输入、输出、发射

着色器开头两行布局是必写的：

```glsl
#version 330 core
layout (points) in;
layout (line_strip, max_vertices = 2) out;
```

输入图元（括号里是这个图元至少有几个顶点）：

| 布局 | 对应的绘制 |
|------|------------|
| `points` | `GL_POINTS`（1） |
| `lines` | `GL_LINES` 或 `GL_LINE_STRIP`（2） |
| `triangles` | `GL_TRIANGLES` / `STRIP` / `FAN`（3） |
| `lines_adjacency` | 带邻接的线（4） |
| `triangles_adjacency` | 带邻接的三角形（6） |

输出只能是三种：`points`、`line_strip`、`triangle_strip`。想要一个三角形，就输出 `triangle_strip` 并发射 3 个顶点。

上个阶段的位置在内建数组里。它是接口块，所以即便只有一个点，下标也要写 `[0]`：

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

## 3. 造房子

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

## 4. 爆破

对模型的每个三角形算一个法线，再沿法线把三个顶点推出去。`sin(time)` 从 -1 收到 1，教程把它映射到 `[0, 1]`，所以三角形只会往外走，再回到原位，不会穿进模型里面。

```glsl
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

vec3 GetNormal()
{
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    return normalize(cross(a, b));
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

这里的法线是用 `gl_Position` 上的三条边叉乘出来的。顶点着色器若已经乘了 `projection * view * model`，叉乘发生在裁剪空间，透视会把方向拉歪。教程用它演示「一个三角形可以整块挪走」。要看法线是否可信，用下一节的可视化，不要用这个叉乘结果当光照法线。

---

## 5. 法线可视化

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

## 6. 和本工程怎么接

| 位置 | 现在 | 和本章的关系 |
|------|------|----------------|
| `Shader::Shader(vs, fs)` | 只创建顶点、片元两个阶段 | 几何着色器要第三段：`GL_GEOMETRY_SHADER`，链接前挂上 |
| `main.cpp` | Day08 四立方体，无几何着色器 | 本章是笔记。房子的输入是 `GL_POINTS`，和立方体的 `GL_TRIANGLES` 不是同一种布局 |
| `Model.h` / `Mesh.h` | 模型顶点含位置、法线、UV | 爆破、法线可视化可以直接用这套属性。法线可视化的第二遍仍画同一个 VAO |

链接顺序：编译三份着色器 → `glAttachShader` 三次 → `glLinkProgram` → 删掉着色器对象。输入布局和 `glDrawArrays` / `glDrawElements` 的模式必须一致，否则几何着色器收不到图元。

---

## 7. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 画面和没写几何着色器时一样，但也没有报错 | 直通着色器就是这样。确认 `GL_GEOMETRY_SHADER` 已经 attach 并重新链接 |
| 什么都看不见 | 输入布局和绘制模式不一致，例如着色器写 `points` 却 `glDrawArrays(GL_TRIANGLES, ...)` |
| 房子缺屋顶或少一条边 | `EmitVertex` 次数超过 `max_vertices`，多出来的被丢弃 |
| 房子形状散成别的三角形 | 三角形带的顶点顺序错了。带是重叠的三个顶点一组，不是每三个顶点一个独立三角形 |
| 颜色整栋房子都是黑的 | 几何着色器里接口块没写成数组，或块名和顶点着色器不一致 |
| 屋顶不是白色 | `fColor = vec3(1.0)` 写在了屋顶那次 `EmitVertex` 之后。发射带走的是调用当时的值 |
| 爆破方向反了 | `cross(a, b)` 的 `a`、`b` 对调了 |
| 法线和物体不跟手、近大远小被拉歪 | 位置已经乘了 `projection` 才进几何着色器。可视化应在观察空间算完，再乘投影 |
| 法线长度夸张 | `MAGNITUDE` 太大。它只影响线段显示长度 |

---

## 8. 小练习

1. `layout (points) in` 时，`gl_in` 里有几个顶点？改成 `triangles` 之后呢？
2. `EmitVertex` 和 `EndPrimitive` 各做一件什么事？只 `EmitVertex` 两次、从不 `EndPrimitive`，线段还会出现吗？
3. 5 个顶点的三角形带会产生几个三角形？顶点顺序为什么是「左下、右下、左上、右上、屋顶」？
4. 为什么几何着色器里的 `in VS_OUT { ... } gs_in[]` 必须是数组，即便点图元只有一个顶点？
5. 法线可视化的顶点着色器为什么不乘 `projection`？投影乘在几何着色器的哪一行？
6. （思考）`max_vertices = 6` 画三条法线刚好用完。若改成 5，第三条线会怎样？

---

## 9. 阅读顺序与下一步

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

## 10. 参考文献

1. LearnOpenGL CN — [几何着色器](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/09%20Geometry%20Shader/)
2. LearnOpenGL EN — [Geometry Shader](https://learnopengl.com/Advanced-OpenGL/Geometry-Shader)
3. 《Part4 Day08 — 高级 GLSL》— 接口块；几何着色器的输入是这块的数组
4. 本工程 `src/headfile/Shader.h` — 目前只链接顶点着色器与片元着色器
