# GLM 常用 API 与类型说明（基于当前项目）

这份文档是 Day 05 的补充，配合《线性代数基础与变换》使用。

- 上一篇讲 **数学概念**（向量、矩阵、MVP 是什么）
- 本篇讲 **GLM 库里怎么写**（类型名、常用函数、和 OpenGL 怎么对接）

当前项目 GLM 位置：

```
Proj/../Depends/glm/     ← GLM（header-only，第三方统一在 Depends）
```

CMake 已通过 `includes/` 配置头文件路径，代码里直接：

```cpp
#include <glm/glm.hpp>
```

---

## 1. GLM 是什么？

**GLM（OpenGL Mathematics）** 是专为图形学设计的 C++ 数学库：

- API 设计 **贴近 GLSL**（`vec3`、`mat4`、`normalize` 等名字几乎一样）
- **Header-only**：只 include，不链接 `.lib`
- 在 CPU 侧算矩阵，再通过 `uniform` 传给 GPU

和 GLSL 的分工：

| 位置 | 用什么 | 做什么 |
|------|--------|--------|
| C++（GLM） | `glm::mat4 model` | 每帧计算变换矩阵 |
| GLSL（Shader） | `uniform mat4 model` | 接收矩阵，变换顶点 |

---

## 2. 常用头文件

不是只 include 一个文件就够，按功能按需引入：

| 头文件 | 用途 |
|--------|------|
| `<glm/glm.hpp>` | 核心：`vec*`、`mat*`、基础运算 |
| `<glm/gtc/matrix_transform.hpp>` | `translate`、`rotate`、`scale`、`lookAt`、`perspective`、`ortho` |
| `<glm/gtc/type_ptr.hpp>` | `value_ptr`，把矩阵/向量指针传给 OpenGL |
| `<glm/gtc/constants.hpp>` | `pi()` 等常量（可选） |

LearnOpenGL 最常见组合：

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
```

---

## 3. 常用类型（变量定义）

GLM 类型和 GLSL **几乎一一对应**。

### 3.1 向量类型

| GLM 类型 | GLSL 对应 | 分量 | 典型用途 |
|----------|-----------|------|----------|
| `glm::vec2` | `vec2` | x, y | UV 纹理坐标、2D 位置 |
| `glm::vec3` | `vec3` | x, y, z | 3D 位置、方向、RGB 颜色 |
| `glm::vec4` | `vec4` | x, y, z, w | 齐次坐标、RGBA 颜色 |

**定义方式：**

```cpp
glm::vec2 uv(0.5f, 0.5f);
glm::vec3 pos(1.0f, 2.0f, 3.0f);
glm::vec4 color(1.0f, 0.0f, 0.0f, 1.0f);

// 或统一初始化
glm::vec3 v{0.0f, 1.0f, 0.0f};
```

**访问分量：**

```cpp
pos.x;  pos.y;  pos.z;
color.r;  color.g;  color.b;  color.a;   // 颜色习惯用 rgba
uv.s;  uv.t;                            // 纹理坐标有时用 st
pos[0];  pos[1];  pos[2];               // 下标访问
```

**swizzle（GLM 也支持部分）：**

```cpp
glm::vec3 rgb = color.rgb;
glm::vec2 xy  = pos.xy;
```

### 3.2 矩阵类型

| GLM 类型 | GLSL 对应 | 大小 | 典型用途 |
|----------|-----------|------|----------|
| `glm::mat2` | `mat2` | 2×2 | 2D 变换（少用） |
| `glm::mat3` | `mat3` | 3×3 | 法线变换、UV 变换 |
| `glm::mat4` | `mat4` | 4×4 | **Model / View / Projection** |

**定义方式：**

```cpp
// 单位矩阵（最常用起点）
glm::mat4 identity = glm::mat4(1.0f);

// 或
glm::mat4 M(1.0f);
```

**注意**：`glm::mat4(1.0f)` 生成 **单位矩阵**，不是「每个元素都是 1」。

**访问元素：**

```cpp
M[0];           // 第 0 列（vec4）
M[0][0];        // 第 0 列第 0 行
```

GLM 默认 **列主序（column-major）**，与 OpenGL 一致。

### 3.3 四元数（进阶，先了解）

| GLM 类型 | 用途 |
|----------|------|
| `glm::quat` | 表示旋转，避免万向锁；高级相机/模型旋转会用 |

初学阶段先用 `glm::rotate` + `mat4` 即可，四元数后面再学。

### 3.4 类型对照总表

```
GLSL          GLM              说明
─────────────────────────────────────────
float         float            标量
vec2          glm::vec2        2 分量向量
vec3          glm::vec3        3 分量向量
vec4          glm::vec4        4 分量向量
mat4          glm::mat4        4×4 矩阵
int           int              整数
bool          bool             布尔
```

---

## 4. 向量常用 API

需 `#include <glm/glm.hpp>`。

### 4.1 基础运算

```cpp
glm::vec3 a(1.0f, 2.0f, 3.0f);
glm::vec3 b(4.0f, 5.0f, 6.0f);

glm::vec3 sum  = a + b;       // 逐分量加
glm::vec3 diff = a - b;       // 逐分量减
glm::vec3 scaled = a * 2.0f;  // 标量乘向量
glm::vec3 mul  = a * b;       // 逐分量乘（Hadamard 积，不是点积）
```

### 4.2 长度与距离

```cpp
float len = glm::length(a);              // |a|
float dist = glm::distance(a, b);          // |a - b|
```

### 4.3 归一化

```cpp
glm::vec3 dir = glm::normalize(a);       // 单位向量，长度为 1
```

### 4.4 点积与叉积

```cpp
float d = glm::dot(a, b);                // 点积，结果是 float
glm::vec3 n = glm::cross(a, b);          // 叉积，结果是 vec3（仅 3D）
```

### 4.5 常用函数速查

| 函数 | 作用 | 返回值 |
|------|------|--------|
| `glm::length(v)` | 向量长度 | `float` |
| `glm::distance(a, b)` | 两点距离 | `float` |
| `glm::normalize(v)` | 归一化 | `vec` |
| `glm::dot(a, b)` | 点积 | `float` |
| `glm::cross(a, b)` | 叉积 | `vec3` |
| `glm::min(a, b)` | 逐分量取小 | `vec` |
| `glm::max(a, b)` | 逐分量取大 | `vec` |
| `glm::clamp(x, min, max)` | 限制范围 | 同类型 |

---

## 5. 矩阵常用 API

### 5.1 基础运算

```cpp
glm::mat4 A(1.0f);
glm::mat4 B(1.0f);

glm::mat4 C = A * B;           // 矩阵乘法（注意顺序）
glm::vec4 v = A * glm::vec4(1, 0, 0, 1);  // 矩阵 × 向量
```

**重要**：`A * B` 表示先应用 B，再应用 A（对列向量从右往左读）。

### 5.2 特殊矩阵

```cpp
glm::mat4 I = glm::mat4(1.0f);           // 单位矩阵
glm::mat4 inv = glm::inverse(M);         // 逆矩阵
glm::mat4 trans = glm::transpose(M);     // 转置
```

---

## 6. 变换 API（最常用）

需 `#include <glm/gtc/matrix_transform.hpp>`。

所有变换函数都是 **左乘风格**：返回新矩阵，第一个参数通常是当前矩阵。

### 6.1 平移 translate

```cpp
glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));
// 沿 X 轴移动 1 个单位
```

### 6.2 旋转 rotate

```cpp
float angle = glm::radians(45.0f);   // 角度转弧度！
model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, 1.0f));
// 绕 Z 轴转 45 度
```

**注意**：第二个参数是 **弧度**，不是角度。用 `glm::radians(度)` 转换。

### 6.3 缩放 scale

```cpp
model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
// 各轴放大 2 倍
```

### 6.4 组合示例（LearnOpenGL 典型写法）

```cpp
glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f),
                    glm::vec3(0.5f, 1.0f, 0.0f));
model = glm::scale(model, glm::vec3(0.5f));
```

对顶点的作用顺序（从右到左）：先 scale → 再 rotate → 再 translate。

### 6.5 视图矩阵 lookAt

```cpp
glm::mat4 view = glm::lookAt(
    glm::vec3(0.0f, 0.0f, 3.0f),    // 相机位置 (eye)
    glm::vec3(0.0f, 0.0f, 0.0f),    // 目标点 (center)
    glm::vec3(0.0f, 1.0f, 0.0f)     // 上方向 (up)
);
```

含义：在 `(0,0,3)` 看向原点，Y 轴朝上。

### 6.6 透视投影 perspective

```cpp
glm::mat4 projection = glm::perspective(
    glm::radians(45.0f),                          // FOV 垂直视野角
    (float)800 / (float)600,                    // 宽高比 aspect
    0.1f,                                         // 近裁剪面 near
    100.0f                                        // 远裁剪面 far
);
```

### 6.7 正交投影 ortho

```cpp
glm::mat4 projection = glm::ortho(
    0.0f, 800.0f,     // left, right
    0.0f, 600.0f,     // bottom, top
    -1.0f, 1.0f       // near, far
);
```

2D / UI 常用；3D 场景一般用 `perspective`。

### 6.8 变换 API 速查

| 函数 | 作用 |
|------|------|
| `glm::translate(M, vec3)` | 平移 |
| `glm::rotate(M, angle, axis)` | 绕轴旋转（弧度） |
| `glm::scale(M, vec3)` | 缩放 |
| `glm::lookAt(eye, center, up)` | 视图矩阵 |
| `glm::perspective(fov, aspect, near, far)` | 透视投影 |
| `glm::ortho(left, right, bottom, top, near, far)` | 正交投影 |

---

## 7. 角度与弧度

GLM 三角函数和 `rotate`、`perspective` 都用 **弧度**。

```cpp
glm::radians(180.0f);    // 度 → 弧度
glm::degrees(glm::pi<float>());  // 弧度 → 度
```

| 角度 | 弧度 |
|------|------|
| 90° | `glm::radians(90.f)` ≈ 1.5708 |
| 180° | `glm::radians(180.f)` ≈ 3.1416 |
| 360° | `glm::radians(360.f)` ≈ 6.2832 |

---

## 8. 和 OpenGL 对接：value_ptr

矩阵在 CPU 算好后，要传给 Shader 的 `uniform mat4`。OpenGL 需要 **float 指针**。

需 `#include <glm/gtc/type_ptr.hpp>`。

### 8.1 传给 glUniformMatrix4fv

```cpp
glm::mat4 model = glm::mat4(1.0f);
// ...

glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(model));
```

| 参数 | 值 | 含义 |
|------|-----|------|
| `location` | uniform 位置 | `glGetUniformLocation` 得到 |
| `count` | `1` | 传 1 个 mat4 |
| `transpose` | `GL_FALSE` | GLM 已是列主序，不要转置 |
| `value` | `glm::value_ptr(model)` | 矩阵数据首地址 |

### 8.2 配合 Shader 类的 setMat4（Day 06 会加）

```cpp
void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(
        glGetUniformLocation(ID, name.c_str()),
        1, GL_FALSE, glm::value_ptr(mat)
    );
}
```

使用：

```cpp
ourShader.setMat4("model", model);
ourShader.setMat4("view", view);
ourShader.setMat4("projection", projection);
```

### 8.3 传给 glBufferData（顶点数据）

顶点数组也可以从 GLM 向量取指针：

```cpp
glm::vec3 vertices[] = { ... };
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), glm::value_ptr(vertices[0]), GL_STATIC_DRAW);
```

---

## 9. 完整 MVP 示例（C++ 侧）

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// 模型：随时间绕 (0.5, 1, 0) 轴旋转
glm::mat4 model = glm::mat4(1.0f);
model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f),
                    glm::vec3(0.5f, 1.0f, 0.0f));

// 视图：相机在 z=3 看向原点
glm::mat4 view = glm::lookAt(
    glm::vec3(0.0f, 0.0f, 3.0f),
    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f)
);

// 投影：45° 视野，800×600，近 0.1 远 100
glm::mat4 projection = glm::perspective(
    glm::radians(45.0f),
    800.0f / 600.0f,
    0.1f, 100.0f
);

// 传入 Shader
ourShader.use();
ourShader.setMat4("model", model);
ourShader.setMat4("view", view);
ourShader.setMat4("projection", projection);
```

对应顶点 Shader：

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
// ...

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

---

## 10. 常见错误

| 现象 | 可能原因 |
|------|----------|
| 物体不转/位置不对 | 角度用了度数没转弧度 |
| 画面拉伸 | `perspective` 的 aspect 没随窗口更新 |
| 物体消失 | near/far 设错，物体在裁剪面外 |
| 矩阵传了没效果 | 忘了 `ourShader.use()` 就 `setMat4` |
| 画面错乱 | `glUniformMatrix4fv` 的 transpose 用了 `GL_TRUE` |
| 编译找不到 glm | include 路径没配或写错 `#include <glm/...>` |
| 变换顺序反了 | `translate`/`rotate` 调用顺序与预期不符 |

---

## 11. 和 Day 05 另一篇文档的关系

| 文档 | 内容 |
|------|------|
| 《线性代数基础与变换》 | 数学概念：向量、矩阵、MVP 是什么 |
| **本文档** | GLM 代码：`glm::vec3`、`glm::rotate`、`value_ptr` 怎么写 |

建议阅读顺序：先数学概念，再 GLM API，最后 Day 06 写代码。

---

## 12. 下一步（Day 06）

1. 在 `Shader.h` 添加 `setMat4`
2. 修改 `3.3.shader.vs` 加入 `uniform mat4 model/view/projection`
3. 在 `main.cpp` 用 GLM 计算 MVP 并传入
4. 窗口 resize 时更新 `projection` 的 aspect  ratio

做到这一步，你的矩形就能 **旋转、透视**，正式进入 3D 变换阶段。
