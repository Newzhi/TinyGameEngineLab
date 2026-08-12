# Assimp 模型导入库（Day 01）

对应 LearnOpenGL Part 3「[Assimp](https://learnopengl-cn.github.io/03%20Model%20Loading/01%20Assimp/)」：**为什么不能再手写顶点数组**，以及用 **Assimp（Open Asset Import Library）** 把建模软件导出的模型统一读进程序。

| 文档 | 内容 |
|------|------|
| Part2 多光源 / 光照贴图 | 箱子是手写立方体顶点 |
| **本文档** | Assimp 是什么、Scene / Node / Mesh、本工程如何本地接入 |
| [Day02 网格 Mesh](../Day02/网格Mesh类.md) | Vertex / Texture / Mesh 类与绘制 |
| [Day03 模型 Model](../Day03/模型Model类.md) | 递归加载、材质贴图、背包 Demo |

> 官网原文写的是 Assimp 3.1.1；本工程使用 **Assimp 5.4.3**（能力与思路相同，API 大方向兼容）。

**前置**：已理解 VAO/VBO/EBO、顶点属性（位置/法线/UV）、材质与贴图。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Model | **模型** | 通常由多个网格组成的完整物体 |
| Mesh | **网格** | 可绘制的最小单位：顶点 + 索引 + 材质 |
| Import | **导入** | 从文件读出数据，变成程序能用的结构 |
| Assimp | **开放资产导入库** | 一种模型格式 → 统一内存结构 |
| Scene | **场景对象** | Assimp 加载后装下整份文件数据的根容器 |
| Node | **节点** | 场景树节点，可挂网格索引、可有子节点 |
| Face | **面** | 一个图元（多为三角形）的顶点索引 |
| Material | **材质** | 颜色、贴图路径等表面属性 |
| UV mapping | **UV 映射** | 把贴图坐标绑到模型表面 |

---

## 1. 为什么需要模型导入？

Part1～Part2 一直用手写的箱子顶点。复杂物体（房子、角色、武器）若仍手填位置/法线/UV，几乎不可能。

常见流程：

```
艺术家用 Blender / 3ds Max / Maya 建模、画贴图
        ↓ 导出
.obj / .fbx / .gltf / Collada … 等文件
        ↓ 我们要做的
解析文件 → 顶点、索引、材质 → OpenGL 的 VBO/EBO/纹理
```

难点：**格式太多**，每种封装方式不同。例如：

| 格式 | 大致特点 |
|------|----------|
| Wavefront `.obj` | 相对简单，顶点和部分材质 |
| Collada `.dae` | XML，可含灯光、动画、整场景 |
| FBX / glTF 等 | 更现代、信息更丰富 |

自己为每一种写导入器成本很高 → 用 **Assimp** 做统一前端。

---

## 2. Assimp 是什么？

**Assimp** = Open Asset Import Library（开放的资产导入库）。

- 能导入大量模型格式（也可导出部分格式）
- 无论源格式是什么，都加载进 **同一套数据结构**
- 你的代码只跟 Assimp 的 Scene / Mesh 打交道，不必关心文件细节

直觉：

```
各种模型文件 ──Assimp──► 统一的 Scene
                              │
                     你的 Mesh / Model 类
                              │
                           OpenGL 绘制
```

---

## 3. Assimp 数据结构（简化）

加载成功后，核心是一个 **`aiScene`（场景）**：

```
Scene（整份导入结果）
├── 根节点 Root Node
│     ├── 子节点 …
│     └── 网格索引（指向 Scene 的网格数组）
├── mMeshes[]     ← 真正的 Mesh 数据
├── mMaterials[]  ← 材质（颜色、贴图路径等）
└── …
```

| 对象 | 作用 |
|------|------|
| **Scene** | 总容器；含网格数组、材质数组、根节点 |
| **Node** | 树节点；可有子节点；`mMeshes` 里是 **网格下标**（不是网格本体） |
| **Mesh** | 顶点位置、法线、UV、面（索引）、材质下标 |
| **Face** | 一个图元；存组成该面的顶点索引（适合 EBO） |
| **Material** | 漫反射色、镜面、贴图文件名等 |

### 3.1 什么是网格（Mesh）？

艺术家很少用「一整块」建人形：头、身体、衣服、武器常是分开的子形状，再组合成模型。  
每个子形状 ≈ 一个 **Mesh**。

- **Mesh**：OpenGL 绘制的最小单位（顶点 + 索引 + 材质）  
- **Model**：多个 Mesh 的集合  

画模型 = 依次画它包含的每个 Mesh（下一节会封装成类）。

### 3.2 加载后我们要做什么（预告）

1. `Importer.ReadFile(...)` 得到 `aiScene*`  
2. 从根节点递归遍历子节点  
3. 按节点上的网格索引取出 `aiMesh`  
4. 抽出顶点、索引、材质/贴图 → 存进自己的 `Mesh` / `Model`  
5. 绑 VAO/VBO/EBO，用现有光照 shader 绘制  

Day01 先把库与结构搞清；Day02/03 再写类与 Demo。

---

## 4. 本工程如何接入 Assimp（已完成）

本仓库 **不采用官网「单独预编译 DLL + 拷贝」为主路径**，而是：

| 项 | 本工程做法 |
|----|------------|
| 源码位置 | `Depends/assimp-5.4.3/` |
| 接入方式 | `Proj/CMakeLists.txt` 里 `add_subdirectory` |
| 链接 | `target_link_libraries(main PRIVATE … assimp …)` |
| 库类型 | 静态（`BUILD_SHARED_LIBS OFF`），一般不必再拷 `assimp.dll` |
| 联网 | **不需要** FetchContent |

CMake 要点（已在工程中）：

```cmake
set(DEPENDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Depends")

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory("${DEPENDS_DIR}/assimp-5.4.3" "${CMAKE_BINARY_DIR}/_deps/assimp")
target_link_libraries(main PRIVATE glad glfw assimp …)
```

代码里将来会写：

```cpp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
```

头文件由 Assimp 的 CMake target 自动提供，**不必**再把 Assimp 头文件复制进 `Proj/includes`。

### 4.1 编译时可能看到的警告

用 **MinGW/GCC** 编 Assimp 时，常出现：

```
ignoring '#pragma warning' [-Wunknown-pragmas]
```

这是 Assimp 依赖里写了 **MSVC 专用** `#pragma warning`，GCC 会忽略——**警告不是错误**，编到 100% 即可。首次编 Assimp 较慢，之后有缓存会快很多。

### 4.2 与官网「自己编 Assimp」的对比

| | LearnOpenGL 官网步骤 | 本工程 |
|--|----------------------|--------|
| 下载 | GitHub 选版本 | 已在 `Depends/assimp-5.4.3` |
| 编译 | 另开 CMake 工程，再拷 lib/dll | 跟主工程一起 `add_subdirectory` |
| 头文件 | 拷到 include | target 自动带上 |
| DirectX 等旧坑 | 手编时可能遇到 | 多数情况可忽略 |

想深入「单独编库再链接」也可以，但对当前小引擎规范，**源码进 Depends + 子目录编译** 更一致。

---

## 5. 和当前项目的关系

| 阶段 | 状态 |
|------|------|
| Assimp 源码与 CMake 链接 | **已接入** |
| `#include` 试编译 / 读文件 | 可随时验证 |
| `Mesh` / `Model` 类 | **下一节** |
| 替换「手写箱子」为导入模型 | 再下一节 |

当前 `main` 仍是多光源木箱 Demo；模型加载代码会逐步加在 `src/headfile`（如已有的 `Mesh.h` 草稿）与 `main` 中。

验证 Assimp 是否链上（可选小实验）：

```cpp
#include <assimp/Importer.hpp>
// 在某处临时：
Assimp::Importer importer;
// 能编译、链接通过即可；真正 ReadFile 放到有模型文件之后
```

---

## 6. 小练习 / 思考

1. 为什么节点里存的是网格 **索引**，而不是直接嵌一整份顶点数据？  
2. 一个人形模型可能有几个 Mesh？画的时候为什么要「按 Mesh 循环」而不是一次 Draw？  
3. `.obj` 和 Collada 对「我们写导入器」的难度差在哪？Assimp 如何抹平？  
4. 打开 `Depends/README.md`，确认 assimp 与 glfw/glad 的放置规则是否一致。

---

## 7. 阅读顺序与下一步

```
Part2 多光源 / 光照贴图
  ↓
Part3 Day01 Assimp（本文）← 库与数据结构、工程接入
  ↓
Part3 Day02 Mesh ← 单个可绘制网格
  ↓
Part3 Day03 Model ← 递归节点、组合多个 Mesh、场景绘制
```

---

## 8. 参考文献

1. LearnOpenGL CN — [Assimp](https://learnopengl-cn.github.io/03%20Model%20Loading/01%20Assimp/)  
2. LearnOpenGL EN — [Assimp](https://learnopengl.com/Model-Loading/Assimp)  
3. Assimp GitHub — https://github.com/assimp/assimp  
4. 本仓库 `Depends/assimp-5.4.3/`、`Depends/README.md`、`Proj/CMakeLists.txt`
