# TinyGameEngineLab - 环境准备与依赖下载说明

本项目建议只提交**项目源码**与 `Depends/glad/`，不要提交 `Depends/cmake-4.3.3/` 和 `Depends/glfw-3.4/` 这类第三方完整源码包。

## 你需要安装/下载的内容

- **Git（必需）**：用于拉取与提交代码  
  下载地址：[Git 官网](https://git-scm.com/downloads)

- **CMake（必需）**：用于生成工程文件（建议安装版，不建议把 CMake 源码放进仓库）  
  下载地址：[CMake 官网](https://cmake.org/download/)

- **GLFW（必需，二选一）**：窗口和输入库  
  - 方案 A（推荐）：本地安装或通过包管理器安装 GLFW  
    官网地址：[GLFW 官网](https://www.glfw.org/download)
  - 方案 B：自行下载 GLFW 源码并在本地编译（不建议直接提交整个源码到仓库）

- **OpenGL 驱动（必需）**：由显卡驱动提供  
  请安装最新显卡驱动：
  - NVIDIA: [NVIDIA Driver Downloads](https://www.nvidia.com/Download/index.aspx)
  - AMD: [AMD Drivers and Support](https://www.amd.com/en/support)
  - Intel: [Intel Driver & Support Assistant](https://www.intel.com/content/www/us/en/support/detect.html)

- **编译器（必需，至少一种）**
  - Windows 推荐：Visual Studio 2022（C++ Desktop Development）  
    下载地址：[Visual Studio](https://visualstudio.microsoft.com/)
  - 或 MinGW-w64（如你使用 GCC 工具链）

## 仓库内已包含内容

- `Depends/glad/`：已包含 OpenGL 加载器代码（通常不需要你再单独生成）。

## 不建议上传到 Git 的内容

- `Depends/cmake-4.3.3/`（CMake 源码，文件量巨大）
- `Depends/glfw-3.4/`（GLFW 完整源码）
- `build/`、`out/`、`cmake-build-*` 等构建输出目录

## Windows 快速检查命令（可选）

在 PowerShell 中执行：

```powershell
git --version
cmake --version
```

若命令可用，说明基础环境已就绪。
