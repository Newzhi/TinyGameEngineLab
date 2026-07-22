# 变换矩阵与 Uniform 传递（基于当前 `main.cpp`）

这份文档对应 Day 05 的 **代码实践**，配合另外两篇阅读：

| 文档 | 内容 |
|------|------|
| 《线性代数基础与变换》 | 向量、矩阵、MVP 的数学概念 |
| 《GLM 常用 API 与类型说明》 | `glm::mat4`、`translate`、`value_ptr` 等 |
| **本文档** | 当前项目里 **如何用 Uniform 传变换矩阵、实现动态旋转** |

---

## 1. Day 04 → Day 05：多了什么？

Day 04 的顶点位置直接写在 NDC 里，矩形位置固定：

```glsl
gl_Position = vec4(aPos, 1.0);
```

Day 05 在 Day 04（纹理矩形）基础上增加：

| 新增 | 作用 |
|------|------|
| GLM 头文件 | CPU 侧计算 `glm::mat4` |
| 顶点 shader 的 `uniform mat4 transform` | GPU 侧对每个顶点做变换 |
| 渲染循环里 `glUniformMatrix4fv` | 把矩阵从 C++ 传给 shader |
| 每帧更新 `trans` | 实现**动态旋转** |

当前数据流：

```
VBO 里的 aPos（固定局部坐标）
        ↓
transform * vec4(aPos, 1.0)   ← uniform 矩阵（每帧可更新）
        ↓
gl_Position → 屏幕上的矩形（可平移/旋转/缩放）
```

---

## 2. 核心问题：顶点在 Shader 里有了，C++ 还要做什么？

**只需要在 C++ 里定义并传递矩阵，不必改 VBO 里的顶点。**

| 数据 | 谁提供 | 在哪 |
|------|--------|------|
| 每个顶点的位置 `aPos` | VBO + Attribute | 已在顶点 shader：`layout(location=0) in vec3 aPos` |
| 变换矩阵 `transform` | Uniform | C++ 算 `glm::mat4 trans`，每帧上传 |

GPU 会对**每个顶点**自动执行：

```glsl
gl_Position = transform * vec4(aPos, 1.0f);
```

你**不需要**在 C++ 里对每个顶点手动做 `trans * vec4(...)`，也不需要每帧改 `vertices[]` 数组。

---

## 3. Uniform 是什么？

**Uniform** 是 GLSL 里由 **CPU 设置、GPU 只读** 的全局变量。

| | Attribute | Uniform |
|---|-----------|---------|
| 典型例子 | `aPos`、`aColor`、`aTexCoord` | `transform`、`ourTexture` |
| 每个顶点是否相同 | **不同** | **相同** |
| C++ 怎么设 | VBO + `glVertexAttribPointer` | `glUniform*` / `Shader::setXxx` |
| 声明 | `layout(location=N) in` | `uniform` |

一次 `glDrawElements` 调用内，所有顶点、所有像素共享同一个 `transform` 值。

---

## 4. Uniform 应该写在哪？

### 4.1 Shader 里：声明

**顶点 shader** `shaders/3.3.shader.vs`：

```glsl
uniform mat4 transform;

void main()
{
    gl_Position = transform * vec4(aPos, 1.0f);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
```

**片段 shader** 里也有 uniform（纹理）：

```glsl
uniform sampler2D ourTexture;
```

名字必须和 C++ 里传的一致（如 `"transform"`、`"ourTexture"`）。

### 4.2 C++ 里：渲染循环中、`use()` 之后传值

```cpp
ourShader.use();                                              // 1. 先启用 shader

unsigned int transformLoc = glGetUniformLocation(ourShader.ID, "transform");
glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));  // 2. 传矩阵

ourShader.setInt("ourTexture", 0);                            // 3. 传纹理单元

glBindVertexArray(VAO);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);          // 4. 再绘制
```

**不要**写在：

- VBO 里（那是 Attribute）
- `glUseProgram` 之前（不知道往哪个 program 写）
- 只想动态变化却只算一次矩阵（见第 7 节）

---

## 5. 当前项目中的两个 Uniform

| Uniform | Shader | C++ 传法 | 作用 |
|---------|--------|----------|------|
| `transform` | 顶点 `.vs` | `glUniformMatrix4fv` | 平移/旋转/缩放顶点 |
| `ourTexture` | 片段 `.fs` | `ourShader.setInt("ourTexture", 0)` | 指定 sampler 用纹理单元 0 |

纹理还需配合：

```cpp
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texture);
ourShader.setInt("ourTexture", 0);
```

`sampler2D` 传的是**纹理单元编号**，不是 `texture` 的 id。

---

## 6. `glUniformMatrix4fv` 逐项解释

当前代码：

```cpp
glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));
```

| 参数 | 值 | 含义 |
|------|-----|------|
| `transformLoc` | `glGetUniformLocation(..., "transform")` | uniform 的位置；为 -1 表示 shader 里没有该变量 |
| `1` | 传 1 个矩阵 | 一次上传一个 `mat4` |
| `GL_FALSE` | 不转置 | GLM 与 OpenGL 都是列主序，必须 `GL_FALSE` |
| `glm::value_ptr(trans)` | 指向 16 个 float | 矩阵在内存中的起始地址 |

等价于告诉 GPU：**用 `trans` 这个 4×4 矩阵去变换每个 `aPos`**。

也可封装进 `Shader.h`（推荐后续添加）：

```cpp
void setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}
// 使用：ourShader.setMat4("transform", trans);
```

---

## 7. 如何实现动态旋转？

### 7.1 错误写法：矩阵在循环外只算一次

```cpp
// ❌ 在 while 之前
glm::mat4 trans = glm::mat4(1.0f);
trans = glm::rotate(trans, (float)glfwGetTime(), ...);

while (...) {
    glUniformMatrix4fv(..., glm::value_ptr(trans));  // 每帧传同一个 trans
}
```

`glfwGetTime()` 只调用一次，角度固定，**屏幕在刷新但物体不动**。

### 7.2 正确写法：矩阵在循环内每帧重算

当前 `main.cpp` 的正确结构：

```cpp
while (!glfwWindowShouldClose(window))
{
    // ✅ 每帧重新计算（时间变化 → 角度变化）
    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::translate(trans, glm::vec3(0.5f, 0.5f, 0.0f));
    trans = glm::rotate(trans, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));
    trans = glm::scale(trans, glm::vec3(0.5f));

    glClear(...);
    ourShader.use();
    glUniformMatrix4fv(..., glm::value_ptr(trans));
    // ... 绘制
}
```

### 7.3 「刷新」≠「动态」

| 每帧发生的事 | 是否变化 |
|-------------|----------|
| `glClear` + `glDrawElements` + `glfwSwapBuffers` | 重复执行（画面在刷新） |
| `trans` 矩阵 | 必须在循环内重算才会变 |

### 7.4 控制转速

`glfwGetTime()` 单位是**秒**，直接当弧度用时旋转较快。可改为：

```cpp
// 每秒旋转 50 度
trans = glm::rotate(trans, (float)glfwGetTime() * glm::radians(50.0f), glm::vec3(0, 0, 1));
```

---

## 8. 变换组合顺序（当前代码）

```cpp
trans = glm::translate(trans, glm::vec3(0.5f, 0.5f, 0.0f));
trans = glm::rotate(trans, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));
trans = glm::scale(trans, glm::vec3(0.5f));
```

对顶点的作用顺序（从右到左）：**先 scale → 再 rotate → 再 translate**。

顺序不同，效果不同。LearnOpenGL 教程通常按 **translate → rotate → scale** 这种写法累积。

---

## 9. 完整数据流图

```
┌─────────────────────────────────────────────────────────────────┐
│  CPU：渲染循环（每帧）                                            │
├─────────────────────────────────────────────────────────────────┤
│  glfwGetTime() → glm::rotate / translate / scale → trans        │
│  ourShader.use()                                                 │
│  glUniformMatrix4fv("transform", trans)                          │
│  setInt("ourTexture", 0) + glBindTexture                         │
│  glDrawElements(...)                                             │
└───────────────────────────┬─────────────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  GPU：顶点 Shader                                                 │
│  aPos ← VBO（Attribute，每顶点不同）                              │
│  transform ← Uniform（整次绘制相同）                              │
│  gl_Position = transform * vec4(aPos, 1.0)                       │
└───────────────────────────┬─────────────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  GPU：片段 Shader                                                 │
│  texture(ourTexture, TexCoord) → FragColor                     │
└───────────────────────────┬─────────────────────────────────────┘
                            ▼
                         屏幕像素
```

---

## 10. 两种传 Uniform 的方式

### 方式 A：OpenGL 原生 API

```cpp
unsigned int loc = glGetUniformLocation(ourShader.ID, "transform");
glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(trans));
```

### 方式 B：Shader 类封装

```cpp
ourShader.setInt("ourTexture", 0);
ourShader.setFloat("mixValue", 0.5f);
// ourShader.setMat4("transform", trans);  // 需在 Shader.h 添加
```

两种方式等价；`setInt` / `setFloat` 内部也是 `glGetUniformLocation` + `glUniform*`。

---

## 11. 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 物体不旋转 | `trans` 在循环外只算一次 | 把矩阵计算挪进 `while` |
| 传了矩阵没效果 | `transformLoc == -1` | 检查 shader 是否声明 `uniform mat4 transform`，名字是否一致 |
| 变换错乱 | `GL_TRUE` 转置 | 改用 `GL_FALSE` |
| 矩形移出屏幕 | `translate(0.5, 0.5, 0)` 在 NDC 下偏右上方 | 改小平移或后续加 view/projection |
| 改了 shader 没变化 | 运行的是构建目录旧副本 | Reload CMake 或改 Working Directory |
| 在 `use()` 前传 uniform | 程序未绑定 | 先 `ourShader.use()` 再 `glUniform*` |

---

## 12. Day 05 文档阅读顺序

```
① 线性代数基础与变换.md     → 理解向量、矩阵、MVP 是什么
② GLM常用API与类型说明.md   → 查 glm::mat4、translate 等 API
③ 本文档                   → 对照 main.cpp 和 shader 写代码
```

---

## 13. 下一步（Day 06，已实现）

Day 06 文档与代码已完成 MVP 立方体，详见：

| 文档 | 内容 |
|------|------|
| `docs/Part1/Day06/坐标系统与空间变换链.md` | 五种坐标空间、MVP 链、视锥体 |
| `docs/Part1/Day06/投影矩阵数学原理.md` | 正交/透视投影矩阵推导 |
| `docs/Part1/Day06/MVP立方体与代码实践.md` | **当前 main.cpp 改动说明** |

当前项目已从单个 `transform` 升级为完整 MVP + 3D 立方体。可继续尝试：

1. 阅读 **`docs/Part1/Day07/摄像机系统概述与原理.md`** — 相机与 View 矩阵原理
2. 按键 WASD 移动相机（每帧更新 `view`）
3. 绘制多个立方体（同一 VAO，不同 `model`）
4. 正交投影对比（`glm::ortho` vs `glm::perspective`）

---

## 14. 当前相关文件清单

```
Proj/
  src/cppfile/main.cpp          ← Day05: transform；Day06: MVP 立方体
  src/headfile/Shader.h         ← setMat4（Day06 新增）
  shaders/3.3.shader.vs         ← Day06: model/view/projection
  shaders/3.3.shader.fs         ← uniform sampler2D ourTexture
  includes/glm/                 ← GLM 1.0.3
  docs/Part1/Day05/                   ← 本文档所在目录
  docs/Part1/Day06/MVP立方体与代码实践.md  ← Day06 代码说明
```
