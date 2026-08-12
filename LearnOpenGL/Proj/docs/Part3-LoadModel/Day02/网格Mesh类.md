# 网格 Mesh 类（Day 02）

对应 LearnOpenGL Part 3「[网格](https://learnopengl-cn.github.io/03%20Model%20Loading/02%20Mesh/)」：把 Assimp 读出的数据，整理成 **OpenGL 能画的一个网格**——顶点、索引、贴图，以及 VAO/VBO/EBO。

| 文档 | 内容 |
|------|------|
| [Day01 Assimp](../Day01/Assimp模型导入库.md) | 为何导入、Scene / Node / Mesh 概念 |
| **本文档** | `Vertex` / `Texture` / `Mesh` 类、`setupMesh`、`Draw` |
| 后续 | Model 类：多个 Mesh + Assimp 递归加载 |

**工程代码**：`Proj/src/headfile/Mesh.h`（头文件内联实现，与教程一致）。

**前置**：VAO/VBO/EBO、`offsetof`、纹理单元、`Shader::setInt`。

---

## 0. 术语中英对照

| 英文 | 中文 | 说明 |
|------|------|------|
| Mesh | **网格** | 一个可 Draw 的实体（顶点+索引+贴图） |
| Vertex | **顶点** | 位置 / 法线 / UV 打包在一起 |
| Index / indices | **索引** | EBO 用，复用顶点 |
| Texture | **纹理（贴图记录）** | GPU 纹理 id + 类型字符串 |
| setupMesh | **初始化网格缓冲** | 创建 VAO/VBO/EBO、设属性指针 |
| Draw | **绘制** | 绑贴图 + `glDrawElements` |
| offsetof | **成员字节偏移** | 算属性在结构体里的起点 |
| Sequential layout | **连续内存布局** | C++ 结构体成员按顺序排布 |

---

## 1. 一个网格最少要什么？

从 Assimp 的 `aiMesh` 转到 OpenGL，至少需要：

| 数据 | 用途 |
|------|------|
| 每个顶点的 **位置** | 几何形状 |
| 每个顶点的 **法线** | 光照 |
| 每个顶点的 **UV** | 采样贴图 |
| **索引** | 索引绘制（三角形） |
| **贴图**（漫反射 / 镜面等） | 材质外观 |

Day02 先定义我们自己的 `Mesh` 类；真正从 Assimp 填数据是下一节 `Model`。

---

## 2. 数据结构

### 2.1 `Vertex`：一个顶点

```cpp
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};
```

三个向量打在一个结构体里，后面整块上传 VBO 更方便。

### 2.2 `Texture`：一张贴图的元数据

```cpp
struct Texture {
    unsigned int id;   // OpenGL 纹理对象
    std::string type;  // "texture_diffuse" 或 "texture_specular"
};
```

（教程后文还会加 `path`，避免同一张图加载两次；本工程 Day02 先保持 id + type。）

### 2.3 `Mesh` 类骨架

```cpp
class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    Mesh(...);
    void Draw(Shader& shader);

private:
    unsigned int VAO, VBO, EBO;
    void setupMesh();
};
```

| 部分 | 职责 |
|------|------|
| 构造函数 | 保存数据，调用 `setupMesh()` |
| `setupMesh` | 建 GPU 缓冲、设顶点布局 |
| `Draw` | 绑贴图、设 sampler uniform、索引绘制 |

把 `Shader` 传给 `Draw`，是为了在绘制前设置「哪个采样器对应哪个纹理单元」。

---

## 3. 为什么结构体适合做 VBO？

C++ 结构体（无虚函数、简单布局时）成员在内存里 **连续排列**。例如：

```
Position(xyz) | Normal(xyz) | TexCoords(uv)
= 8 个 float = 32 字节（常见情况）
```

因此可以：

```cpp
glBufferData(GL_ARRAY_BUFFER,
             vertices.size() * sizeof(Vertex),
             vertices.data(),   // 等价于 &vertices[0]
             GL_STATIC_DRAW);
```

一次上传整网顶点，不必拆成三个数组。

属性偏移用宏：

```cpp
offsetof(Vertex, Normal)     // 法线相对结构体开头的字节数
offsetof(Vertex, TexCoords)  // UV 的字节偏移
```

步长一律 `sizeof(Vertex)`。以后若加切线等字段，多半只需改结构体 + 多一条 `glVertexAttribPointer`，布局逻辑不变。

对应顶点着色器布局（与 Part2 箱子一致）：

| location | 含义 | 大小 |
|----------|------|------|
| 0 | 位置 | vec3 |
| 1 | 法线 | vec3 |
| 2 | UV | vec2 |

---

## 4. `setupMesh` 在做什么？

流程与手写立方体相同，只是数据来自 `vector`：

```
1. glGen VAO / VBO / EBO
2. 绑 VAO
3. VBO ← 全部 Vertex
4. EBO ← 全部 indices
5. 属性 0/1/2：位置、法线、UV（stride = sizeof(Vertex)）
6. 解绑 VAO
```

之后每次 `Draw` 只需 `glBindVertexArray(VAO)` + `glDrawElements`。

---

## 5. `Draw`：贴图怎么绑？

问题：事先不知道有几张漫反射、几张镜面贴图。

约定命名（与教程一致）：

```
texture_diffuse1, texture_diffuse2, ...
texture_specular1, texture_specular2, ...
```

本工程 `Draw` 里拼成：

```
material.texture_diffuse1
material.texture_specular1
...
```

循环逻辑：

```
diffuseNr = 1, specularNr = 1
对每张 textures[i]：
  激活纹理单元 i
  若 type 是 texture_diffuse  → 序号 = diffuseNr++
  若 type 是 texture_specular → 序号 = specularNr++
  setInt("material." + type + 序号, i)
  绑定 textures[i].id
最后 glDrawElements(...)
```

注意：`variable++` 是 **先用当前值再加一**，所以第一张漫反射得到的是 `1`。

着色器里需要有对应的 `sampler2D`（数量按你实际用到的定义）。若网格贴图少于采样器数量，多出来的采样器不用即可。

---

## 6. 和本工程代码的对应

| 文件 / 符号 | 说明 |
|-------------|------|
| `src/headfile/Mesh.h` | 完整 Mesh 实现（含注释） |
| `#include "Shader.h"` | 使用本工程 Shader 类（不是教程的 `learnopengl/shader.h`） |
| `Shader::setInt` | 设置采样器 → 纹理单元编号 |
| CMake | 头文件在 `src/headfile`，已被 include；**不必**把 Mesh.h 加进 `glad` target |

当前 `main` 仍是手写箱子 + 多光源；等有了 `Model` 再 `meshes[i].Draw(shader)`。

### 6.1 与 Part2 光照贴图的衔接

Part2 箱子固定：

```
material.diffuse  → 单元 0
material.specular → 单元 1
```

Mesh 更通用：同一网格可有多张漫反射/镜面，用 **带序号的名字**。  
写模型用的 fragment shader 时，采样器命名要和 `Draw` 里 `setInt` 的字符串一致，否则贴不上。

---

## 7. 小练习

1. 用纸画出一个 `Vertex` 的内存布局，标出 `Normal`、`TexCoords` 的偏移（字节）。  
2. 若只有 1 张漫反射、0 张镜面，`Draw` 循环会 `setInt` 什么名字、绑到哪个单元？  
3. （思考）为什么 `Draw` 要传入 `Shader&`，而不是在 Mesh 内部写死 program id？  
4. （可选）给 `Texture` 增加 `std::string path`，为下一节「同路径不重复加载」做准备。

---

## 8. 阅读顺序与下一步

```
Day01 Assimp（库与 Scene）
  ↓
Day02 Mesh（本文）← 单个可绘制网格
  ↓
Model 类 ← 多个 Mesh + Assimp 遍历节点/材质
```

---

## 9. 参考文献

1. LearnOpenGL CN — [网格](https://learnopengl-cn.github.io/03%20Model%20Loading/02%20Mesh/)  
2. LearnOpenGL EN — [Mesh](https://learnopengl.com/Model-Loading/Mesh)  
3. 本工程 `src/headfile/Mesh.h`、`Shader.h`  
4. 《Part3 Day01 — Assimp》— Mesh 在 Assimp 中的含义  
