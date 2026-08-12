# Uniform 原理与实现机制：为什么 CPU 能把变量传进 Shader？

对应问题：**为什么 `uniform` 可以把 C++ 里的变量传给 GPU 上的着色器？底层是怎么实现的？**

本文讲清「数据通路 + API 两步曲」，并指向本仓库已有文档与 `Shader.h` 源码。

| 文档 | 内容 |
|------|------|
| [EBO 与 Uniform 颜色](./EBO索引绘制与Uniform颜色.md) | Uniform 入门：Attribute vs Uniform、`glUniform4f` 改整体色 |
| [Shader 类封装](./Shader类封装与项目结构整理.md) | `setFloat` / `setVec3` 等封装 |
| [变换矩阵与 Uniform 传递](../Day05/变换矩阵与Uniform传递.md) | `mat4`、`glUniformMatrix4fv` |
| [OpenGL 核心理解](../OpenGL核心理解_数据与执行流程.md) | Uniform / Attribute / Varying 总览 |
| **本文档** | **为何能传、实现链路、`Shader.h` / `main.cpp` / `lightTest.fs` 原码解析** |

---

## 1. 一句话结论

Uniform **不是魔法**：它是 OpenGL **着色器程序（Program）上的一组命名槽位**。

1. GLSL 里写 `uniform xxx` → 链接 Program 时，驱动为每个名字分配 **location（槽号）**；
2. C++ 用 `glGetUniformLocation` 查槽号，再用 `glUniform*` 把 CPU 内存里的值 **写入当前绑定 Program 的该槽**；
3. 之后 `glDraw*` 时，VS/FS 执行到读这个变量，GPU 就从该槽读到你刚写入的值。

所以「能传变量」= **先约定名字与类型（shader 声明）→ 再通过 OpenGL API 往 Program 状态里写数**。

---

## 2. 和 Attribute 的分工（为什么要有 Uniform）

| | Attribute（`in`） | Uniform |
|---|-------------------|---------|
| 数据从哪来 | VBO（顶点缓冲） | `glUniform*`（CPU 直接写 Program） |
| 一次 Draw 内 | **每个顶点可不同** | **所有顶点/片元共享同一值** |
| 典型用途 | 位置、法线、UV | MVP、灯光、材质、`lightType`、时间 |
| 改一次影响 | 一个顶点 | 整次绘制 |

灯光位置、相机、矩阵若放进 VBO，每个顶点都要重复同一份数据，又慢又浪费。  
**整次绘制相同的参数 → Uniform**；**随顶点变化的数据 → Attribute**。

---

## 3. 实现链路（从声明到屏幕）

```
① GLSL 声明
   uniform vec3 light.position;
   uniform int  lightType;
   uniform mat4 model;
        ↓
② 编译 + 链接成 Program（Shader 构造函数）
   glCreateShader → glCompileShader
   glCreateProgram → glAttachShader → glLinkProgram
   链接后：驱动解析所有 active uniform，分配 location
        ↓
③ 绑定当前要改的 Program
   glUseProgram(ID)          ← Shader::use()
        ↓
④ 查名字 → location，再写入
   loc = glGetUniformLocation(ID, "lightType")
   glUniform1i(loc, 2)       ← Shader::setInt(...)
        ↓
⑤ 绘制
   glDrawArrays(...)
   GPU 执行 FS：读到 lightType == 2 → 走聚光分支
```

关键点：

- **没有**「C++ 变量指针直接映射到 GLSL 变量」这种语言级共享内存；
- 有的是 **按名字查槽 + 显式拷贝数值** 进驱动管理的 Program 状态；
- 名字必须 **字符串完全一致**（含 `material.diffuse`、`light.position` 这种成员路径）。

---

## 4. 两个核心 API（实现就靠它们）

### 4.1 `glGetUniformLocation(program, name)`

- 输入：已链接的 `program` ID、GLSL 里的 **uniform 名字**；
- 输出：整数 **location**；找不到或未使用（被优化掉）则返回 **-1**；
- 对 `-1` 调 `glUniform*` 会被忽略（静默失败，画面不变——调试时很常见）。

Struct 成员写法示例：

```
light.position
light.direction
material.shininess
material.diffuse     // sampler2D：传的是纹理单元编号 int
```

### 4.2 `glUniform*`（必须先 `glUseProgram`）

写入 **当前绑定** 的 Program 的某个 location。常用对应关系：

| C++ / Shader.h | OpenGL | GLSL |
|----------------|--------|------|
| `setInt` | `glUniform1i` | `int` / `bool` / `sampler2D` |
| `setFloat` | `glUniform1f` | `float` |
| `setVec3` | `glUniform3f` | `vec3` |
| `setMat4` | `glUniformMatrix4fv` | `mat4` |

`glUniform*` **不**把 `program` 当参数——它作用于 **当前** `glUseProgram` 选中的那个。  
因此顺序必须是：

```
shader.use();           // 1. 绑定
shader.setInt(...);     // 2. 写入该 Program 的 uniform
glDrawArrays(...);      // 3. 绘制时 GPU 读到新值
```

---

## 5. 原码解析：`Shader.h`（封装层）

路径：`src/headfile/Shader.h`。Uniform 能力分三块：**造 Program → use 绑定 → setXxx 写槽**。

### 5.1 构造：编译 + 链接，Uniform 槽在此时诞生

```18:68:LearnOpenGL/Proj/src/headfile/Shader.h
    Shader(const char* vertexPath, const char* fragmentPath)
    {
        // ... 读 .vs / .fs 源码到字符串 ...

        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, nullptr);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, nullptr);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }
```

| 步骤 | API | 和 Uniform 的关系 |
|------|-----|-------------------|
| 编译 VS/FS | `glCompileShader` | 解析 `uniform` 声明的类型与名字 |
| 链接 | `glLinkProgram` | 合并 VS+FS，为 **active uniform** 分配 **location** |
| 得到 `ID` | `glCreateProgram` | 之后所有 `glGetUniformLocation(ID, …)` 都针对这个对象 |

链接成功后，「`lightType` 对应几号槽」才确定。构造函数本身**不写**任何 uniform 值，只准备好可写的 Program。

### 5.2 `use()`：指定「往哪个 Program 写」

```70:73:LearnOpenGL/Proj/src/headfile/Shader.h
    void use() const
    {
        glUseProgram(ID);
    }
```

`glUniform*` **没有 program 参数**，只改「当前绑定」的 Program。  
因此：**先 `use()`，再 `setXxx`**。场景里有 `lightingShader` 和 `lampShader` 两个 Program，各自有自己的 uniform 表，切 `use` 等于换表。

### 5.3 `setInt`：一行拆开就是「查槽 + 写 int」

```80:83:LearnOpenGL/Proj/src/headfile/Shader.h
    void setInt(const std::string& name, int value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
```

等价手写：

```cpp
int loc = glGetUniformLocation(ID, name.c_str()); // 名字 → location
glUniform1i(loc, value);                          // 写入当前 Program
```

用途：

- `uniform int lightType`
- `uniform sampler2D material.diffuse`（传的是纹理单元号 `0`/`1`，不是像素）
- `setBool` 也是走 `glUniform1i`（GLSL bool 用 0/1）

```75:78:LearnOpenGL/Proj/src/headfile/Shader.h
    void setBool(const std::string& name, bool value) const
    {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), static_cast<int>(value));
    }
```

### 5.4 `setFloat` / `setVec3`：标量与向量

```85:98:LearnOpenGL/Proj/src/headfile/Shader.h
    void setFloat(const std::string& name, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec2(const std::string& name, float x, float y) const
    {
        glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
    }

    void setVec3(const std::string& name, float x, float y, float z) const
    {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
    }
```

| 方法 | OpenGL | GLSL 例子（当前工程） |
|------|--------|----------------------|
| `setFloat` | `glUniform1f` | `light.cutOff`、`material.shininess` |
| `setVec3` | `glUniform3f` | `light.position`、`viewPos`、`light.ambient` |
| `setVec4` | `glUniform4f` | 颜色 `rgba`（本 Demo 较少用） |

注意：struct 成员必须写 **全名字符串** `"light.position"`，不能只传 `"light"`。

### 5.5 `setMat4`：矩阵要指针 + 不转置

```105:109:LearnOpenGL/Proj/src/headfile/Shader.h
    void setMat4(const std::string& name, const glm::mat4& mat) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }
```

| 参数 | 含义 |
|------|------|
| `1` | 上传 1 个矩阵 |
| `GL_FALSE` | 不转置（GLM 已是列主序，与 OpenGL 一致） |
| `glm::value_ptr(mat)` | 得到连续 16 个 `float` 的首地址 |

对应 VS 里的 `uniform mat4 model/view/projection`。

### 5.6 对照表（原码 ↔ API）

| `Shader.h` | 底层 | 写入类型 |
|------------|------|----------|
| `use()` | `glUseProgram(ID)` | — |
| `setBool` / `setInt` | `glUniform1i` | int / bool / sampler |
| `setFloat` | `glUniform1f` | float |
| `setVec2/3/4` | `glUniform2/3/4f` | vec* |
| `setMat4` | `glUniformMatrix4fv` | mat4 |

---

## 6. 原码解析：GLSL 声明侧（`lightTest.fs`）

CPU 只能给「shader 里已经声明、且链接后仍 active」的名字赋值。接收端：

```8:33:LearnOpenGL/Proj/shaders/lightTest.fs
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float     shininess;
};

struct Light {
    vec3  position;
    vec3  direction;
    float cutOff;
    float outerCutOff;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;
uniform int lightType; // 0 平行光 | 1 点光 | 2 聚光
```

| GLSL | C++ 应调用的名字 | 方法 |
|------|------------------|------|
| `lightType` | `"lightType"` | `setInt` |
| `viewPos` | `"viewPos"` | `setVec3` |
| `light.position` | `"light.position"` | `setVec3` |
| `material.shininess` | `"material.shininess"` | `setFloat` |
| `material.diffuse` | `"material.diffuse"` | `setInt(0)` → 纹理单元 0 |

FS 里使用示例（读的就是刚写入的槽）：

```37:41:LearnOpenGL/Proj/shaders/lightTest.fs
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    vec3 texDiff = texture(material.diffuse, TexCoords).rgb;
    vec3 texSpec = texture(material.specular, TexCoords).rgb;
```

`viewPos`、`material.diffuse` 都来自 uniform；`TexCoords` 来自 Attribute 插值，不是 uniform。

---

## 7. 原码解析：调用侧（`main.cpp`）

### 7.1 启动时：采样器 + shininess（设一次即可）

```221:224:LearnOpenGL/Proj/src/cppfile/main.cpp
    lightingShader.use();
    lightingShader.setInt("material.diffuse", 0);
    lightingShader.setInt("material.specular", 1);
    lightingShader.setFloat("material.shininess", 64.0f);
```

含义：告诉 FS「`material.diffuse` 从纹理单元 0 采样，specular 从单元 1」。  
真正绑图在渲染循环里 `glActiveTexture` + `glBindTexture`。

### 7.2 每帧：灯光相关 uniform

```64:93:LearnOpenGL/Proj/src/cppfile/main.cpp
void applyLightUniforms(const Shader& shader, const Camera& camera)
{
    shader.setInt("lightType", g_lightType);
    shader.setVec3("light.ambient",  lightColor.x * 0.1f, lightColor.y * 0.1f, lightColor.z * 0.1f);
    shader.setVec3("light.diffuse",  lightColor.x * 0.8f, lightColor.y * 0.8f, lightColor.z * 0.8f);
    shader.setVec3("light.specular", lightColor.x, lightColor.y, lightColor.z);

    shader.setFloat("light.constant",  1.0f);
    shader.setFloat("light.linear",    0.09f);
    shader.setFloat("light.quadratic", 0.032f);

    shader.setFloat("light.cutOff",      std::cos(glm::radians(12.5f)));
    shader.setFloat("light.outerCutOff", std::cos(glm::radians(17.5f)));

    if (g_lightType == 0)
        shader.setVec3("light.direction", lightDir.x, lightDir.y, lightDir.z);
    else if (g_lightType == 1)
        shader.setVec3("light.position", lightPos.x, lightPos.y, lightPos.z);
    else {
        shader.setVec3("light.position",  camera.Position.x, camera.Position.y, camera.Position.z);
        shader.setVec3("light.direction", camera.Front.x, camera.Front.y, camera.Front.z);
    }
}
```

按键改 `g_lightType` → `setInt("lightType", …)` → FS `if (lightType == …)` 分支切换平行光/点光/聚光。  
这就是「C++ 变量进 Shader」的完整实例。

### 7.3 每帧绘制：先 use，再设矩阵，再 Draw

```253:272:LearnOpenGL/Proj/src/cppfile/main.cpp
        lightingShader.use();
        applyLightUniforms(lightingShader, camera);
        lightingShader.setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("projection", projection);
        // ... 绑纹理 ...
        for (int i = 0; i < cubeCount; ++i)
        {
            // ... 算每个箱子的 model ...
            lightingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
```

| 顺序 | 代码 | 作用 |
|------|------|------|
| 1 | `use()` | 绑定 lighting Program |
| 2 | `applyLightUniforms` / `viewPos` / `view`/`projection` | 整帧共享的灯光与相机 |
| 3 | 循环里 `setMat4("model")` | **每个箱子不同**，仍用 uniform（每 Draw 前改一次） |
| 4 | `glDrawArrays` | GPU 用当前槽里的值跑 VS/FS |

换灯立方体时必须再 `lampShader.use()`，并给 **lamp** 自己的 `lightColor` / MVP 赋值——两个 Program 的 uniform **互不相通**：

```278:281:LearnOpenGL/Proj/src/cppfile/main.cpp
            lampShader.use();
            lampShader.setVec3("lightColor", lightColor.x, lightColor.y, lightColor.z);
            lampShader.setMat4("view", view);
            lampShader.setMat4("projection", projection);
```

### 7.4 一条数据通路串起来（以 `lightType` 为例）

```
g_lightType = 2                    // C++ 全局变量（按键 3）
    ↓
applyLightUniforms:
  setInt("lightType", 2)
    ↓
Shader.h:
  glGetUniformLocation(ID, "lightType") → loc
  glUniform1i(loc, 2)
    ↓
Program 的 uniform 槽写入 2
    ↓
glDrawArrays → lightTest.fs:
  if (lightType == 2) { /* 聚光 */ }
```

---

## 8. 「为什么叫 Uniform / 统一」？

一次 Draw Call 内，所有顶点、所有片元看到的该变量 **值统一**。  
换一批物体时，你可以改 `model` 再画一次；换光源模式时改 `lightType`——都是改 Program 状态后再 Draw。

对比三种「传数据」方式：

| 通道 | 机制 | 何时用 |
|------|------|--------|
| Attribute | VBO → 顶点拉取 | 每顶点不同 |
| Uniform | `glUniform*` → Program 槽 | 每 Draw 共享 |
| Texture | 绑定纹理 + `sampler2D` uniform 指单元 | 二维/大数据采样 |

`sampler2D` 本身也是一种 uniform：你传的不是像素，而是 **纹理单元编号**（0、1…），真正像素在 `glBindTexture` 绑到该单元的纹理对象里。

---

## 9. 常见坑（实现层面）

| 现象 | 原因 |
|------|------|
| 设了没效果 | 未 `use()`；名字拼错；location == -1；shader 里变量被优化掉未用 |
| struct 设不上 | 应用 `light.ambient` 这种 **成员全名**，不是只写 `light` |
| 矩阵花屏 | `setMat4` 的 transpose 误用 `GL_TRUE`；或未 `value_ptr` |
| 切 Program 后旧值「没了」 | uniform 存在于 **各自 Program**；换 `lamp` shader 要重新设它自己的 uniform |
| 每帧 `glGetUniformLocation` | 能跑但偏慢；进阶可缓存 location（当前封装为简洁每次查询） |

---

## 10. 和驱动 / GPU 的直观模型（不必记死）

可以把已链接的 Program 想成：

```
Program #ID
  ├── 可执行 VS / FS 机器码
  └── Uniform 表：
        "model"      → location 0 → 16 个 float
        "lightType"  → location 3 → 1 个 int
        "light.position" → ...
```

`glUniform*` = 往表里某格 **写内存**；Draw = GPU 跑 shader 时 **读这些格**。  
具体存在驱动的哪块缓冲、是否进 constant buffer，由实现决定，对应用层只需记住 **API 契约**。

---

## 11. 阅读顺序建议

```
Day03《EBO与Uniform颜色》     ← 第一次接触：改一个颜色
  ↓
本文《Uniform原理与实现机制》 ← 为何能传、两步 API、原码解析
  ↓
Day03《Shader类封装》         ← setXxx 对照表
  ↓
Day05《变换矩阵与Uniform传递》← mat4 / MVP
  ↓
Part2 材质 / 光照 / 投光物    ← struct uniform、lightType 等大量应用
```

---

## 12. 参考文献

1. LearnOpenGL CN — [着色器](https://learnopengl-cn.github.io/01%20Getting%20started/05%20Shaders/)（Uniform 小节）
2. LearnOpenGL CN — [着色器类](https://learnopengl-cn.github.io/01%20Getting%20started/05%20Shaders/#shader)（`setXxx` 封装思路）
3. OpenGL Wiki — [Uniform (GLSL)](https://www.khronos.org/opengl/wiki/Uniform_(GLSL))
4. 本仓库 `src/headfile/Shader.h`、`src/cppfile/main.cpp`、`shaders/lightTest.fs` — 本节原码
5. 《EBO索引绘制与Uniform颜色》《变换矩阵与Uniform传递》— 用法实例
