# TinyGameEngineLab - 构建说明（离线依赖）

第三方库统一放在仓库根下的 **`Depends/`**，由 CMake `add_subdirectory` / include 接入。
**不再使用 FetchContent 联网下载**。

## 需要准备的环境

- **CMake**：建议 3.17+（由系统或 CLion 提供，不要放进 `Depends/`）
- **C/C++ 编译器**：Windows 下可用 MSVC 或 MinGW
- **不强制网络**：`Depends/` 内已含 glfw、glad、glm、stb、assimp 源码

## Depends 中的依赖

| 目录 | 用途 |
|------|------|
| `Depends/glad/` | OpenGL 函数加载 |
| `Depends/glfw/` | 窗口与输入 |
| `Depends/glm/` | 数学库（header-only） |
| `Depends/stb/` | 图像加载（stb_image） |
| `Depends/assimp-5.4.3/` | 模型导入 |

详见 `Depends/README.md`。

## 克隆后构建步骤（CLion）

1. 用 CLion 打开项目中的 `Proj` 目录。
2. 确认 `Toolchains` 里 CMake 与编译器可用。
3. 点击 **Reload CMake Project**（首次会本地编译 glfw / assimp，可能较慢）。
4. 运行目标 `main`。

## 备注

- Windows 上链接 `opengl32`；非 Windows 使用 `find_package(OpenGL)`。
- 工具链安装包（若有）放在 `Tools/`，不要放进 `Depends/`。

## 学习文档

- 总览与核心流程：`Proj/docs/Part1-Base/OpenGL核心理解_数据与执行流程.md`
- Part 1 入门笔记：`Proj/docs/Part1-Base/README.md`
- Part 2 光照笔记：`Proj/docs/Part2-Lighting/README.md`
- Part 3 模型加载：`Proj/docs/Part3-LoadModel/README.md`
