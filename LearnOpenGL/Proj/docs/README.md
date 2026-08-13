# TinyGameEngineLab - 文档与构建说明

第三方依赖在仓库根下的 **`Depends/`**。主工程 **`Proj/`** 通过预编译库（`Depends/prebuilt/`）链接 GLFW / Assimp，**不再**在每次构建时 `add_subdirectory` 整份 Assimp 源码。

## 需要准备的环境

- **CMake**：建议 3.17+（由系统或 CLion 提供，不要放进 `Depends/`）
- **C/C++ 编译器**：本工程预编译库针对 **CLion 自带 MinGW**；换编译器大版本后需按 `Depends/README.md` 重新生成 `prebuilt/`
- **不强制网络**：依赖源码与预编译产物均在本地

## Depends 中的依赖

| 目录 | 用途 |
|------|------|
| `Depends/glad/` | OpenGL 函数加载（本工程直接编 `glad.c`） |
| `Depends/glfw/` | 窗口与输入（源码；已装进 `prebuilt/`） |
| `Depends/glm/` | 数学库（header-only） |
| `Depends/stb/` | 图像加载（stb_image） |
| `Depends/assimp-5.4.3/` | 模型导入（源码；已装进 `prebuilt/`） |
| `Depends/prebuilt/` | 预编译头文件 + 静态库（`.a`） |

详见 `Depends/README.md`。

## 克隆后构建步骤（CLion）

1. 用 CLion 打开项目中的 **`Proj`** 目录。
2. 确认 `Toolchains` 里 CMake 与 MinGW 可用。
3. 确认已有 `Depends/prebuilt/lib`（`libassimp.a`、`libglfw3.a`、`libzlibstatic.a`）；没有则按 `Depends/README.md` 生成。
4. 点击 **Reload CMake Project**（现在应较快，不再配置整份 Assimp）。
5. **Build** 并运行目标 `main`（工作目录一般为 `cmake-build-debug`）。

## 备注

- Windows 上链接 `opengl32`、`gdi32`；非 Windows 使用 `find_package(OpenGL)`。
- 工具链安装包（若有）放在 `Tools/`，不要放进 `Depends/`。
- 改着色器后点 **Build** 即可（POST_BUILD 会同步 `shaders/`）；不必为改 `.fs` 去 Reload。

## 学习文档

| 部分 | 路径 |
|------|------|
| **Part 0 工程与构建基础（建议先读）** | `Proj/docs/Part0-CppBuild/README.md` |
| Part 1 入门 | `Proj/docs/Part1-Base/README.md` |
| OpenGL 数据与执行流程 | `Proj/docs/Part1-Base/OpenGL核心理解_数据与执行流程.md` |
| Part 2 光照 | `Proj/docs/Part2-Lighting/README.md` |
| Part 3 模型加载 | `Proj/docs/Part3-LoadModel/README.md` |
| Part 4 高级 OpenGL | `Proj/docs/Part4-HighLevelOpenGL/README.md` |
