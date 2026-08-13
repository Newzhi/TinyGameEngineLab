# CMake 与本工程部署（Day 03）

前面两天：知道了编译/链接，也知道了头文件和库。  
这一天回答：**谁在指挥这一切？本仓库目录怎么摆？你在 CLion 里该点哪个按钮？**

| 文档 | 内容 |
|------|------|
| [Day01](../Day01/从源码到可执行程序.md) | `.cpp` → `.exe` |
| [Day02](../Day02/头文件与静态库动态库.md) | 头文件与库 |
| **本文档** | CMake、`CMakeLists.txt`、本工程布局、日常操作 |

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| CMake | **CMake** | 读说明书、生成真正构建文件的工具（不是编译器） |
| CMakeLists.txt | **CMake 脚本** | 工程的「构建说明书」 |
| Configure / Reload | **配置 / 重新加载** | 让 CMake 重新读 `CMakeLists.txt` |
| Generator | **生成器** | 生成 Makefile 还是 Ninja 等 |
| Target | **目标** | 如可执行文件 `main`、库 `glad` |
| Toolchain | **工具链** | 编译器 + CMake + 相关工具（CLion 里配置） |
| Working directory | **工作目录** | 程序运行时「当前路径」，相对路径从这儿算 |

---

## 1. CMake 是什么？（不是编译器）

很多人第一次会以为：CMake = 编译。其实：

```
你写 CMakeLists.txt
        │
        ▼  CMake 配置（Configure）
生成 Makefile / Ninja 等真正构建脚本
        │
        ▼  Build（调用 g++ / 链接器）
得到 main.exe
```

| 角色 | 干什么 |
|------|--------|
| **你** | 写 `.cpp`、写 `CMakeLists.txt` |
| **CMake** | 读说明书，决定「编哪些文件、找哪些头、链哪些库」 |
| **编译器 g++** | 真正把 `.cpp` 变成 `.o` |
| **链接器** | 把 `.o` 和 `.a` 拼成 `.exe` |
| **CLion** | 帮你点按钮：Reload CMake / Build / Run |

类比：CMake 是「项目经理」；g++ 是「工人」。

---

## 2. `CMakeLists.txt` 在说什么？（对照本工程）

文件：`Proj/CMakeLists.txt`。挑最常用的几句：

### 2.1 工程与语言版本

```cmake
cmake_minimum_required(VERSION 3.17)
project(learn_opengl_proj LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
```

意思：至少 CMake 3.17；这是个 C/C++ 工程；用 C++17。

### 2.2 告诉编译器：头文件在哪

```cmake
target_include_directories(main PRIVATE
    .../src/headfile
    .../Depends/prebuilt/include
    .../Depends          # glm
    .../Depends/stb
)
```

没有这一步，`#include <glad/glad.h>`、`#include <assimp/...>` 会报「找不到」。

### 2.3 生成可执行文件

```cmake
add_executable(main
    src/cppfile/main.cpp
    src/cppfile/stb_image_impl.cpp
    ...
)
```

意思：用这些源文件做出一个叫 `main` 的程序。

### 2.4 链接库

```cmake
target_link_directories(main PRIVATE ${PREBUILT_DIR}/lib)
target_link_libraries(main PRIVATE glad glfw3 assimp zlibstatic opengl32 gdi32)
```

意思：到 `prebuilt/lib` 找 `.a`，把这些库链进 `main`。

### 2.5 资源拷贝

```cmake
# 每次 Build 后同步 shaders
add_custom_command(TARGET main POST_BUILD ...)

# 配置时拷一次 Resource
file(COPY Resource DESTINATION ...)
```

保证运行目录里能找到着色器和模型贴图。

---

## 3. 本仓库目录地图

```
LearnOpenGL/
├── Depends/                 第三方（不要塞进 Proj/src）
│   ├── glad/                本工程当场编译
│   ├── glfw/                源码（已预编译）
│   ├── assimp-5.4.3/        源码（已预编译）
│   ├── glm/  stb/           头文件库
│   ├── prebuilt/            ★ 预编译产物（include + lib）
│   └── README.md            如何重新生成 prebuilt
├── Tools/                   工具链安装包（若有），不是业务代码
└── Proj/                    ★ 你的主工程（CLion 通常打开这里）
    ├── CMakeLists.txt
    ├── src/
    ├── shaders/
    ├── Resource/
    ├── docs/                学习笔记（含本文 Part0）
    └── cmake-build-debug/   构建产物（一般不提交 git）
```

和 LearnOpenGL **官方**思路对比：

| 官方仓库 | 本工程 |
|----------|--------|
| `includes/` + `lib/` + 有时 `dlls/` | `Depends/prebuilt/include` + `lib`（静态，少管 dll） |
| `find_package` / 链现成库 | 链 `prebuilt` 里的 `.a` |
| 不把 Assimp 源码每次编进示例 | 同样：**主工程不再 `add_subdirectory` Assimp/GLFW 源码** |

---

## 4. CLion 日常：该点哪个？

| 你想做的事 | 点什么 | 会不会很慢 |
|------------|--------|------------|
| 改了 `main.cpp` / 头文件里的逻辑 | **Build** | 通常只编你的代码 + 链接 |
| 改了 `shaders/*.fs` | **Build**（会 POST_BUILD 同步） | 很快 |
| 改了 `CMakeLists.txt`、换依赖方式 | **Reload CMake Project** | 现在应较快（不再配置整份 Assimp） |
| 工具链坏了、缓存乱了 | Reset Cache and Reload（慎用） | 可能较慢，一般别当日常操作 |

**经验**：日常改业务代码，**不要频繁 Reload**。  
只有改了「构建说明书」才 Reload。

运行前确认：

1. Run Configuration 的目标是 `main`
2. 工作目录一般是 `cmake-build-debug`（相对路径 `Resource/...`、`shaders/...` 才找得到）

---

## 5. 为什么有时「改一个数字也很慢」？

即使已经用了预编译 Assimp，改一行 `main.cpp` 仍可能要几秒到十几秒，常见原因：

1. **`main.cpp` 很「重」**：它 `#include "Model.h"`，而 `Model.h` 又 include 了整套 Assimp 头 → 编译器要解析大量头文件。  
2. **链接**：Debug 下 `main.exe` 体积不小，链接本身要时间。  
3. **误点了 Reload**：又重新跑了一遍配置（现在轻很多，但仍没必要天天 Reload）。

这和「有没有预编译库」是两件事：

- 预编译库 → 解决 **Reload / 全量重编第三方**  
- 预编译头（PCH）或把 `Model` 实现拆到 `.cpp` → 解决 **改一行也编译很重**

教程为了好抄，常把 `Model` 全写在头文件里；工程上可以以后再优化。

---

## 6. 编辑器报「找不到 iostream / GLFW」？

若 **Build 能成功**，但编辑器一片红：

多半是 **索引** 问题，不是程序真编不过。

本工程开了：

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

会在构建目录生成 `compile_commands.json`，供 clangd 知道头文件路径。  
Reload 一次 CMake 后，若仍红，可重启语言服务或重开项目。

---

## 7. 小练习

1. 用自己的话解释：CMake 和 g++ 谁真正编译 `.cpp`？  
2. `target_include_directories` 和 `target_link_libraries` 分别解决哪一类错误（找不到头？找不到符号？）？  
3. 为什么本工程把 Assimp 放进 `Depends/prebuilt`，而不是每次 `add_subdirectory(assimp源码)`？  
4. 打开 `Proj/CMakeLists.txt`，指出「生成 main」「找头文件」「链接库」各对应哪几行。

---

## 8. 阅读顺序与下一步

```
Part0 Day01～Day03（构建基础）
  ↓
Part1 Base（窗口、三角形、着色器……）
  ↓
Part2 Lighting
  ↓
Part3 LoadModel
```

第三方细节与重新生成预编译库的命令：`LearnOpenGL/Depends/README.md`。

---

## 9. 参考文献

1. 本工程 `Proj/CMakeLists.txt`、`Depends/README.md`  
2. [CMake 官方文档](https://cmake.org/documentation/)（需要时再查，不必一上来通读）  
3. LearnOpenGL 官方仓库根目录 `CMakeLists.txt`（对比「用现成库」的思路）  
4. 《Part0 Day01 / Day02》  
