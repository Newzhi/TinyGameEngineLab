# Shader 类封装与项目结构整理

这份文档记录 Day 03 做的两件工程性改动：

1. **把项目文件按职责拆进不同目录**，让结构更清晰
2. **把原来写在 `main.cpp` 里的字符串 Shader，抽成单独的 `Shader` 类 + 独立 `.vs`/`.fs` 文件**

---

## 1. 目录结构对比

### Day 02（旧）

```
Proj/
  src/
    main.cpp        ← 顶点/片段着色器源码用字符串硬写在这里
    glad.c
  includes/
    glad/glad.h
    GLFW/glfw3.h
    KHR/khrplatform.h
  CMakeLists.txt
```

### Day 03（现在）

```
Proj/
  src/
    glad.c                  ← 第三方 GLAD 加载器（C 源文件）
    cppfile/
      main.cpp              ← 程序入口，只负责"用"Shader，不再写 GLSL 字符串
    headfile/
      Shader.h              ← Shader 类定义（header-only）
  includes/
    glad/glad.h
    GLFW/glfw3.h
    KHR/khrplatform.h
  shaders/
    3.3.shader.vs           ← 顶点着色器 GLSL 源文件
    3.3.shader.fs           ← 片段着色器 GLSL 源文件
  docs/
    Day03/
      EBO索引绘制与Uniform颜色.md
      Shader类封装与项目结构整理.md  ← 本文档
  CMakeLists.txt
```

### 每个目录的职责

| 目录 | 放什么 | 原则 |
|------|--------|------|
| `src/cppfile/` | `.cpp` 程序源文件 | 需要被编译的 C++ 翻译单元 |
| `src/headfile/` | 项目自己写的 `.h` 头文件 | 只被 `#include`，不直接参与编译 |
| `src/` 根目录 | `glad.c`（第三方 C 源文件） | 暂时单独放，后续可移到 `third_party/` |
| `includes/` | 外部库头文件（GLAD、GLFW、KHR） | 第三方只读，不修改 |
| `shaders/` | GLSL 着色器文件 | 运行时读取，不参与 C++ 编译 |

---

## 2. 为什么要把 GLSL 从字符串里拆出来？

Day 02 的顶点着色器是这样的：

```cpp
// main.cpp 里的字符串拼接——很难维护
const char* vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "out vec3 ourColor;\n"
    "void main() { ... }\0";
```

**问题：**

- 没有语法高亮，GLSL 写错不容易发现
- 修改 Shader 需要重新编译 C++ 代码
- GLSL 和 C++ 逻辑混在同一个文件里，越写越乱
- 字符串末尾手动加 `\0` 容易遗漏

**Day 03 的做法：** 把 GLSL 单独存成文件，运行时由 `Shader` 类读进来。

`shaders/3.3.shader.vs`：

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
}
```

`shaders/3.3.shader.fs`：

```glsl
#version 330 core
out vec4 FragColor;

in vec3 ourColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
```

独立 `.vs`/`.fs` 文件的好处：IDE 能识别 GLSL 语法并高亮，改 Shader 不用重新编译 C++，文件名直接说明用途。

---

## 3. Shader 类：为什么这样设计？

### 3.1 为什么是 header-only（只有 `.h` 没有 `.cpp`）？

`Shader.h` 把实现直接写在类体内，没有单独的 `Shader.cpp`。

原因：`Shader` 类目前只有一个文件（`main.cpp`）用到，不需要多个翻译单元共享编译结果。header-only 更简单——只要 `#include "Shader.h"`，不需要在 CMake 里单独加编译文件。

等后面项目大了，多个 `.cpp` 都用到 `Shader`，再拆成 `.h` + `.cpp` 也不迟。

### 3.2 include 防护（Header Guard）

```cpp
#ifndef SHADER_H
#define SHADER_H
// ...
#endif // SHADER_H
```

防止同一个翻译单元重复 `#include "Shader.h"` 导致类被定义两次从而报错。这是所有 `.h` 文件的标准写法。

### 3.3 为什么要 `#include <glad/glad.h>`？

`Shader.h` 里直接调用了 `glCreateShader`、`glCompileShader`、`glUseProgram` 等 OpenGL 函数。这些函数的指针声明在 `glad.h` 里，不包含就找不到。

`glad.h` 必须在 `glfw3.h` **之前** include（GLFW 里有些宏定义与 GLAD 的 OpenGL 声明有冲突），`Shader.h` 里写了 GLAD 就不需要调用者再操心顺序。

### 3.4 构造函数：读文件 → 编译 → 链接

```cpp
Shader(const char* vertexPath, const char* fragmentPath)
```

整个构造流程分三段：

**第一段：从文件读 GLSL 源码**

```cpp
std::ifstream vShaderFile;
vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
try {
    vShaderFile.open(vertexPath);
    std::stringstream vShaderStream;
    vShaderStream << vShaderFile.rdbuf();   // 把整个文件倒进字符流
    vertexCode = vShaderStream.str();       // 转成 std::string
} catch (const std::ifstream::failure&) {
    std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
}
```

- `exceptions(failbit | badbit)`：提前设置哪些错误会抛异常（打开失败、读取 I/O 错误），这样 `try/catch` 能捕获文件不存在的情况
- `rdbuf()`：直接把文件的底层缓冲区倒进 `stringstream`，比逐行读取更快
- 注意：**文件路径是相对于程序运行时的工作目录**，不是相对于源文件。CMake 会把 `shaders/` 目录复制到构建输出目录，所以运行时能找到

**第二段：编译两个 Shader 对象**

```cpp
unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(vertex, 1, &vShaderCode, nullptr);
glCompileShader(vertex);
checkCompileErrors(vertex, "VERTEX");
```

这和 Day 02 的 `compileShader()` 函数逻辑完全一样，只是改成了类的私有方法 `checkCompileErrors`。

**第三段：链接成 Program，并释放临时 Shader 对象**

```cpp
ID = glCreateProgram();
glAttachShader(ID, vertex);
glAttachShader(ID, fragment);
glLinkProgram(ID);
checkCompileErrors(ID, "PROGRAM");

glDeleteShader(vertex);    // 链接完成后 GPU 不再需要这两个中间对象
glDeleteShader(fragment);
```

构造函数结束后，`ID` 就是这个着色器程序的句柄，可以直接用。

### 3.5 `use()` 方法

```cpp
void use() const { glUseProgram(ID); }
```

把 `glUseProgram(shaderProgram)` 封装成一行。加 `const` 是因为调用它不会改变 `Shader` 对象本身的任何成员。

### 3.6 Uniform 设置方法

```cpp
void setFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}
```

每个 `setXxx` 把「查 location + 设值」合并成一步，调用时直接：

```cpp
ourShader.setFloat("brightness", 0.8f);
```

比手写两行更简洁，也不容易因为 program 没激活就查 location 而出 bug（`use()` 之后调就没问题）。

当前支持的类型：

| 方法 | 对应 GLSL 类型 | 对应 OpenGL API |
|------|--------------|----------------|
| `setBool` | `bool` | `glUniform1i` |
| `setInt` | `int` | `glUniform1i` |
| `setFloat` | `float` | `glUniform1f` |
| `setVec2` | `vec2` | `glUniform2f` |
| `setVec3` | `vec3` | `glUniform3f` |
| `setVec4` | `vec4` | `glUniform4f` |

注意：当前版本的 `setVec2/3/4` 接受分开的 `float` 参数而不是 `glm::vec*`，因为项目目前还没引入 GLM 库。后续 Day 04 引入变换矩阵时再加 `setMat4` 版本。

### 3.7 私有方法 `checkCompileErrors`

```cpp
void checkCompileErrors(unsigned int shader, const std::string& type) const
```

把「查编译/链接状态 + 打印日志」抽成独立私有方法，避免在构造函数里重复写三遍相似代码。`type` 传 `"VERTEX"`/`"FRAGMENT"`/`"PROGRAM"` 用来区分是哪个阶段失败：Shader 和 Program 用的查询 API 不一样（`glGetShaderiv` vs `glGetProgramiv`），所以需要分支。

---

## 4. main.cpp 为什么这样写？

### 4.1 include 顺序

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../headfile/Shader.h"
#include <iostream>
```

- `glad.h` 必须在 `glfw3.h` 之前（GLAD 先声明 OpenGL 符号，避免冲突）
- `Shader.h` 用 `""` 相对路径，因为是项目内部文件；`<>` 尖括号留给系统/第三方库
- `../headfile/Shader.h`：从 `cppfile/main.cpp` 出发，上一层是 `src/`，再进 `headfile/`

### 4.2 Shader 对象必须在 GLAD 初始化之后创建

```cpp
// 先初始化 GLAD
if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { ... }

// 然后才能创建 Shader（内部会调用 glCreateShader 等 OpenGL API）
Shader ourShader("shaders/3.3.shader.vs", "shaders/3.3.shader.fs");
```

`Shader` 构造函数内部调用 `glCreateShader`、`glCompileShader` 等 OpenGL 函数。这些函数指针由 GLAD 在 `gladLoadGLLoader` 时填入——如果在这之前创建 `Shader`，函数指针是空的，会直接崩溃。

### 4.3 shader 文件路径是运行时路径，不是源码路径

```cpp
Shader ourShader("shaders/3.3.shader.vs", "shaders/3.3.shader.fs");
```

这里的 `"shaders/..."` 是**相对于程序运行时工作目录**的路径，不是相对于 `main.cpp` 所在目录。CMakeLists.txt 里有：

```cmake
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
```

这行会在每次 CMake 配置时把 `Proj/shaders/` 整个目录复制到构建输出目录（如 `cmake-build-debug/shaders/`）。运行时工作目录默认就是构建目录，所以能找到。

### 4.4 渲染循环里只需两行调用

```cpp
ourShader.use();          // 激活着色器程序
glBindVertexArray(VAO);
glDrawArrays(GL_TRIANGLES, 0, 3);
```

`main.cpp` 里不再出现任何 `glUseProgram` 的 ID 数字、也不再出现 GLSL 字符串。Shader 的内部细节全部封装在 `Shader.h` 里。

---

## 5. CMakeLists.txt 改动说明

```cmake
add_executable(main
    src/glad.c
    src/cppfile/main.cpp
    src/headfile/Shader.h       ← 加进来只是让 IDE 能索引，不影响实际编译
)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/includes    ← GLAD/GLFW 等第三方头文件
)

# 把 shaders/ 目录复制到构建输出目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
```

`Shader.h` 写进 `add_executable` 不是必须的（CMake 编译时会通过 `#include` 自动找到），加在这里只是让 CLion 的项目树能显示这个文件，方便导航。

`src/headfile/` 没有加进 `target_include_directories`——因为 `main.cpp` 用的是相对路径 `"../headfile/Shader.h"`，编译器会从 `main.cpp` 所在目录出发自动找，不需要额外配置。

---

## 6. Day 03 前后对比

| | Day 02 | Day 03 |
|---|--------|--------|
| GLSL 存放位置 | C++ 字符串，写在 `main.cpp` 里 | 独立 `.vs` / `.fs` 文件 |
| 编译链接逻辑 | `main.cpp` 里的两个全局函数 | `Shader` 类的构造函数 |
| 渲染循环里的 Shader 操作 | `glUseProgram(shaderProgram)` | `ourShader.use()` |
| Uniform 设置 | 手写 `glGetUniformLocation` + `glUniform*` | `ourShader.setFloat(...)` |
| 项目文件结构 | 所有文件平铺在 `src/` | 按职责分进 `cppfile/`、`headfile/`、`shaders/` |
| 改 Shader 是否重新编译 C++ | 是 | 否（改 `.vs`/`.fs` 不触发 C++ 重编） |
