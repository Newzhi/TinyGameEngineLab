# Part 3 — Model Loading

LearnOpenGL「模型加载」阶段学习笔记，按天组织：

| 目录 | 主题 |
|------|------|
| [Day01](./Day01/) | Assimp：为何需要模型导入库、数据结构、本工程接入方式 |
| [Day02](./Day02/) | Mesh 类：Vertex / Texture、setupMesh、Draw 与贴图命名约定 |
| [Day03](./Day03/) | Model 类：Assimp 递归加载、材质贴图、背包 Demo、UV/贴图翻转 |

建议先完成 Part2（多光源、光照贴图），再进入本部分。

第三方依赖统一在仓库 `Depends/`（离线源码，不联网 FetchContent）。Assimp 版本见 `Depends/assimp-5.4.3/`。

## 快速入口

| 文档 | 文件 |
|------|------|
| Assimp | [Day01/Assimp模型导入库.md](./Day01/Assimp模型导入库.md) |
| Mesh | [Day02/网格Mesh类.md](./Day02/网格Mesh类.md) |
| Model | [Day03/模型Model类.md](./Day03/模型Model类.md) |

测试资源：`Proj/Resource/TestLoadModel/`（`backpack.obj` + diffuse/specular/normal 等）。
