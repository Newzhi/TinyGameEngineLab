# TinyGameEngineLab - 构建说明（可直接克隆使用）

项目已改为 **CMake FetchContent 自动下载 GLFW**。因此仓库中不需要提交 `Depends/glfw-3.4/`，克隆后只要具备基础工具即可构建。

## 需要准备的环境

- **Git**：用于克隆代码（`git clone`）
- **CMake**：建议 3.17+
- **C/C++ 编译器**：Windows 下可用 MSVC 或 MinGW
- **网络连接**：首次配置时 CMake 会自动拉取 GLFW 源码

## 已内置依赖

- `Depends/glad/`：已包含 `glad.c` 和相关头文件

## 克隆后构建步骤（CLion）

1. 用 CLion 打开项目中的 `Proj` 目录。
2. 确认 `Toolchains` 里 CMake 与编译器可用。
3. 点击 **Reload CMake Project**。
4. 首次会自动下载并构建 GLFW，随后可直接运行目标 `main`。

## 备注

- Windows 上链接 `opengl32`；非 Windows 平台使用 `find_package(OpenGL)`。
- 如果是离线环境，可改回本地第三方目录方式（手动提供 GLFW 源码/库）。

