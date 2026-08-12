# 模型 Model 类（Day 03）

对应 LearnOpenGL Part 3「[模型](https://learnopengl-cn.github.io/03%20Model%20Loading/03%20Model/)」：用 Assimp 读入整份模型文件，递归遍历节点，把每个 `aiMesh` 转成我们的 `Mesh`，再统一 `Draw`。

| 文档 | 内容 |
|------|------|
| [Day01 Assimp](../Day01/Assimp模型导入库.md) | Scene / Node / Mesh 概念、工程接入 |
| [Day02 Mesh](../Day02/网格Mesh类.md) | 单个可绘制网格 |
| **本文档** | `Model` 类、材质贴图加载、背包 Demo、UV/贴图翻转 |

**工程代码**：

| 文件 | 作用 |
|------|------|
| `src/headfile/Model.h` | Model + `TextureFromFile` |
| `src/headfile/Mesh.h` | Vertex / Texture / Mesh |
| `shaders/model.vs` / `model.fs` | 采样 `material.texture_diffuse1` 等 |
| `Resource/TestLoadModel/` | 背包 `.obj` + 贴图 |
| `main.cpp` | 构造 `Model` 并每帧 `Draw` |

**前置**：已理解 Assimp Scene、Mesh 的 VAO/贴图命名约定、Part2 多光源。

---

## 0. 术语中英对照

| 英文 | 中文 | 说明 |
|------|------|------|
| Model | **模型** | 多个 Mesh + 共享贴图缓存 |
| loadModel | **加载模型** | Assimp `ReadFile` + 取目录 + 处理根节点 |
| processNode | **处理节点** | 本节点网格 → 递归子节点 |
| processMesh | **处理网格** | `aiMesh` → 我们的 `Vertex`/`indices`/`Texture` |
| loadMaterialTextures | **加载材质贴图** | 按 `aiTextureType` 取路径并上传 GPU |
| textures_loaded | **已加载贴图表** | 同路径去重，避免重复 `stbi_load` |
| directory | **模型所在目录** | 用来拼贴图相对路径 |
| aiProcess_FlipUVs | **翻转 UV** | 把 V 变成 `1-V`，对齐 OpenGL |
| stbi flip | **翻转图像行** | 图片原点在左上，OpenGL 纹理原点在左下 |

---

## 1. Model 和 Mesh 的分工

```
Model（资源级）
  ├── directory          ← "Resource/TestLoadModel"
  ├── textures_loaded[]  ← 整模型共享，按 path 去重
  └── meshes[]           ← 每个对应一个 aiMesh
        └── Mesh::Draw(shader)
```

| 类 | 管什么 |
|----|--------|
| `Mesh` | 「一块可画的几何」：缓冲 + 本块贴图列表 |
| `Model` | 「从文件装进来」：Assimp 遍历、材质→贴图、多 Mesh 聚合 |

调用方只需要：

```cpp
Model backpack("Resource/TestLoadModel/backpack.obj");
// ...
backpack.Draw(modelShader);
```

---

## 2. 加载流程总览

```
路径 path（正斜杠）
    │
    ▼
Assimp::Importer::ReadFile(path, 后处理 flags)
    │  失败 → 打印 GetErrorString，return
    ▼
directory = path 去掉最后一段文件名
    │
    ▼
processNode(root, scene)
    │
    ├─ 对本节点每个 mesh 索引：
    │      processMesh → meshes.push_back
    └─ 对每个子节点递归 processNode
```

本工程后处理组合（与教程一致）：

```cpp
aiProcess_Triangulate      // 面 → 三角形
| aiProcess_GenSmoothNormals
| aiProcess_FlipUVs        // UV.V 翻转
| aiProcess_CalcTangentSpace // 切线/副切线（填 Vertex.Tangent/Bitangent）
```

---

## 3. `processNode`：场景树怎么走？

Assimp 里：

- **`aiScene`**：整份导入结果（所有 mesh / material 数组）
- **`aiNode`**：树节点；`mMeshes[]` 存的是 **场景里 mesh 的下标**，不是顶点本身

因此：

```cpp
aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
meshes.push_back(processMesh(mesh, scene));
```

再 `processNode(children[i], scene)`。  
一个模型可能有多个节点、多个网格（身体、背包带、扣环……），最终都进 `Model::meshes`。

---

## 4. `processMesh`：aiMesh → Mesh

对每个顶点填：

| 字段 | 来源 |
|------|------|
| `Position` | `mVertices` |
| `Normal` | `mNormals`（有则填） |
| `TexCoords` | `mTextureCoords[0]`；没有则 `(0,0)` |
| `Tangent` / `Bitangent` | `mTangents` / `mBitangents`（有 UV 且算过切线空间时） |

索引：遍历每个 `aiFace`，把 `mIndices` 推进 `indices`（三角化后一般每面 3 个）。

材质：用 `mesh->mMaterialIndex` 取 `aiMaterial*`，再按类型加载贴图：

| Assimp 类型 | 我们的 `Texture.type` | 用途 |
|-------------|----------------------|------|
| `aiTextureType_DIFFUSE` | `texture_diffuse` | 漫反射 / 固有色 |
| `aiTextureType_SPECULAR` | `texture_specular` | 镜面强度 |
| `aiTextureType_HEIGHT` | `texture_normal` | 教程约定：部分模型把法线图放在 Height 槽 |
| `aiTextureType_AMBIENT` | `texture_height` | 高度图（可选） |

最后：

```cpp
return Mesh(vertices, indices, textures);
```

`Mesh` 构造里立刻 `setupMesh()`，GPU 缓冲建好。

---

## 5. 贴图加载与去重

### 5.1 `loadMaterialTextures`

对材质某类型的每一张：

1. `mat->GetTexture` 得到相对文件名（如 `diffuse.jpg`）
2. 在 `textures_loaded` 里按 `path` 查找 → 已有则复用 `Texture`（含同一个 GPU id）
3. 否则 `TextureFromFile` → 填 `id/type/path`，推进本 mesh 与 `textures_loaded`

### 5.2 `TextureFromFile`

```
filename = directory + '/' + 材质里的相对名
stbi_load → glTexImage2D → mipmap → 返回 textureID
```

失败会打印 `Texture failed to load at path: ...`（几何仍可画，可能是黑/灰）。

### 5.3 Windows 路径

`directory` 用 `path.find_last_of('/')` 截取。  
**请统一写正斜杠**：

```text
Resource/TestLoadModel/backpack.obj   ✅
Resource\TestLoadModel\backpack.obj   ❌ 可能截错目录，贴图全挂
```

运行目录一般是 `cmake-build-debug`；CMake 已 `file(COPY Resource ...)`，相对路径从构建目录起算。

---

## 6. 为什么贴图要「翻转」？（必读）

图片文件原点在 **左上**，OpenGL 纹理坐标原点在 **左下**。两边约定不同，原样上传容易上下颠倒。

本工程采用教程配对：

| 步骤 | 作用 |
|------|------|
| `aiProcess_FlipUVs` | UV 的 V → `1-V` |
| `stbi_set_flip_vertically_on_load(true)` | 像素行上下翻 |

**两次翻转互相抵消 → 采样正确。**  
只做一边（例如只 FlipUVs、stbi 设 `false`）就会上下反。

注意：`stbi_set_flip_vertically_on_load` 是 **全局状态**。  
`main` 里箱子的 `loadTexture` 也会设 `true`；`TextureFromFile` 里应显式设成你期望的值，避免调用顺序导致行为漂移。

等价方案（二选一，不要混）：

- 翻 UV + 翻图（当前 / 教程）
- 都不翻

---

## 7. 着色器约定：`model.vs` / `model.fs`

`Mesh::Draw` 会设置：

```text
material.texture_diffuse1
material.texture_specular1
（若有）material.texture_normal1 ...
```

因此模型用的 FS **不能**再写 Part2 箱子的 `material.diffuse` / `material.specular`，而要：

```glsl
struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
    float shininess;
};
```

`model.vs` 与 Mesh 属性布局一致：

| location | 含义 |
|----------|------|
| 0 | Position |
| 1 | Normal |
| 2 | TexCoords |
| 3 | Tangent（当前 VS 可未用） |
| 4 | Bitangent（当前 VS 可未用） |

本工程 `model.fs` 复用 Part2 **多光源**（平行光 + 4 点光 + 聚光），只是采样器名字换成上面约定。

法线贴图：`Model` 会加载 `normal.png`（`texture_normal`），但当前 `model.fs` **尚未采样**；完整切线空间光照是后续进阶内容。

---

## 8. 本工程背包 Demo

### 8.1 资源目录

```
Resource/TestLoadModel/
  backpack.obj
  backpack.mtl
  diffuse.jpg      ← map_Kd
  specular.jpg     ← map_Ks
  normal.png       ← map_Bump
  ao.jpg           ← mtl 未引用，当前不会自动加载
  roughness.jpg    ← 同上（偏 PBR）
  source_attribution.txt
```

`backpack.mtl` 实际引用的是 diffuse / specular / normal 三张。  
`ao` / `roughness` 留在目录里，要进管线需改材质加载或手写 PBR shader。

### 8.2 `main` 用法摘要

```cpp
Shader modelShader("shaders/model.vs", "shaders/model.fs");
Model backpackModel("Resource/TestLoadModel/backpack.obj");

// 循环里：
modelShader.use();
applyMultipleLights(modelShader, camera);
modelShader.setMat4("view", view);
modelShader.setMat4("projection", projection);
modelShader.setFloat("material.shininess", 32.0f);
glm::mat4 modelMat(1.0f);
// translate / scale 按需要调
modelShader.setMat4("model", modelMat);
backpackModel.Draw(modelShader);
```

箱子仍用 `lightTest` + `material.diffuse`；模型用 `model` + `texture_diffuse1`。两套 shader 并存是刻意的。

---

## 9. FBX / 其他格式

Assimp 默认编入多种导入器（含 **FBX**）。API 相同：

```cpp
Model m("Resource/SomeFolder/model.fbx");
```

相对 `.obj` 常见坑：

| 问题 | 说明 |
|------|------|
| 贴图路径 | 可能是绝对路径、外置目录或嵌在 FBX 内；当前拼接 `directory + '/' + name` 可能找不着 |
| 材质槽 | 现代资源常用 BaseColor 等；你们主要认 DIFFUSE/SPECULAR |
| 动画/蒙皮 | Assimp 能读，当前 `Model` **只画静态网格** |

格式上能导；「看起来对」取决于贴图引用与材质槽是否对上现有逻辑。

---

## 10. 和本工程代码的对应

| 符号 / 文件 | 说明 |
|-------------|------|
| `Model::loadModel` | `ReadFile` + 截目录 + `processNode` |
| `Model::Draw` | 对每个 `meshes[i].Draw(shader)` |
| `Texture::path` | 去重键 |
| `CMakeLists.txt` | `Mesh.h`/`Model.h` 挂在 `main`；**不要**塞进 `glad` |
| `file(COPY Resource ...)` | 构建目录可访问模型与贴图 |

---

## 11. 小练习

1. 画出 `backpack.obj` 从 `ReadFile` 到第一次 `glDrawElements` 的调用链。  
2. 若删掉 `aiProcess_FlipUVs` 但保留 `stbi` flip `true`，画面会怎样？为什么？  
3. 同一张 `diffuse.jpg` 被两个 mesh 引用时，`textures_loaded` 起什么作用？  
4. （可选）把相机拉近背包，用 `scale` 调到合适大小；观察有无 `Texture failed to load`。  
5. （思考）若要支持嵌入式 FBX 贴图，`TextureFromFile` 还缺什么？

---

## 12. 阅读顺序与下一步

```
Day01 Assimp（库与 Scene）
  ↓
Day02 Mesh（单个网格）
  ↓
Day03 Model（本文）← 文件 → 多 Mesh → 场景里画出来
  ↓
进阶：法线贴图 / PBR（ao、roughness）/ 动画蒙皮 / 资源与组件拆分
```

LearnOpenGL 官网 Part3 到 Model 基本结束；后面章节是高级光照与特效，不再扩 Assimp API。

---

## 13. 参考文献

1. LearnOpenGL CN — [模型](https://learnopengl-cn.github.io/03%20Model%20Loading/03%20Model/)  
2. LearnOpenGL EN — [Model](https://learnopengl.com/Model-Loading/Model)  
3. 本工程 `Model.h`、`Mesh.h`、`shaders/model.*`、`Resource/TestLoadModel/`  
4. 《Part3 Day01 / Day02》— Assimp 与 Mesh 基础  
