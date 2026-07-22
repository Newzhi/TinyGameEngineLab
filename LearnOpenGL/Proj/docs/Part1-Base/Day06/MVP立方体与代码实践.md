# MVP 立方体与代码实践（基于当前 `main.cpp`）

这份文档对应 Day 06 的**代码实践**，配合另外两篇数学阅读：

| 文档 | 内容 |
|------|------|
| 《坐标系统与空间变换链》 | 五种坐标空间、MVP 链、视锥体 |
| 《投影矩阵数学原理》 | 正交/透视投影矩阵推导 |
| **本文档** | 当前项目里 **如何从 2D 矩形升级到 3D 旋转立方体** |

---

## 1. Day 05 → Day 06：改了什么？

Day 05 在 NDC 平面上画一个**纹理矩形**，只有一个 `transform` uniform：

```glsl
gl_Position = transform * vec4(aPos, 1.0);
```

Day 06 升级为完整的 **MVP 管线 + 3D 立方体**：

| 变更 | 之前（Day 05） | 现在（Day 06） |
|------|---------------|---------------|
| 几何体 | 4 顶点矩形 + EBO | 36 顶点立方体，无 EBO |
| 绘制 API | `glDrawElements` | `glDrawArrays` |
| Shader uniform | 单个 `transform` | `model` / `view` / `projection` |
| 坐标空间 | 顶点接近 NDC | 局部空间 `[-0.5, 0.5]³` + 完整 MVP |
| 3D 支持 | 无 | `glEnable(GL_DEPTH_TEST)` |
| 相机 | 无 | `glm::lookAt` |
| 投影 | 无 | `glm::perspective` |
| Shader 传矩阵 | `glUniformMatrix4fv` 手写 | `Shader::setMat4` |

运行效果：窗口中显示贴有 `container.jpg` 纹理的**立方体**，随时间绕斜轴旋转。

---

## 2. 整体数据流

```
VBO 里的 aPos（局部空间，立方体中心在原点）
        ↓
model      ← 每帧更新（倾斜 + 随时间旋转）
        ↓
view       ← 相机在 (0,0,3) 看向原点
        ↓
projection ← 45° 透视，aspect 随窗口变化
        ↓
gl_Position（裁剪空间）→ GPU 透视除法 → 屏幕
```

对应 Shader 一行：

```glsl
gl_Position = projection * view * model * vec4(aPos, 1.0f);
```

---

## 3. 修改的文件清单

```
Proj/
  src/cppfile/main.cpp          ← 立方体顶点、MVP 矩阵、深度测试
  src/headfile/Shader.h         ← 新增 setMat4
  shaders/3.3.shader.vs           ← transform → model/view/projection
  shaders/3.3.shader.fs           ← 未改（仍采样纹理 × 顶点色）
  Resource/Texture/container.jpg  ← 六个面共用同一张纹理
  docs/Part1/Day06/
    坐标系统与空间变换链.md
    投影矩阵数学原理.md
    MVP立方体与代码实践.md        ← 本文档
```

---

## 4. 顶点着色器改动

**之前：**

```glsl
uniform mat4 transform;
gl_Position = transform * vec4(aPos, 1.0f);
```

**现在：**

```glsl
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

gl_Position = projection * view * model * vec4(aPos, 1.0f);
```

| uniform | 含义 | C++ 侧何时计算 |
|---------|------|----------------|
| `model` | 物体自身的平移/旋转/缩放 | **渲染循环内**（每帧旋转） |
| `view` | 世界 → 相机 | 循环外（相机固定） |
| `projection` | 相机 → 裁剪（透视） | 循环内（窗口 resize 时 aspect 会变） |

---

## 5. Shader.h：新增 `setMat4`

Day 05 用手写 `glUniformMatrix4fv` 传单个矩阵；Day 06 封装为：

```cpp
void setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}
```

渲染循环里调用：

```cpp
ourShader.setMat4("model", model);
ourShader.setMat4("view", view);
ourShader.setMat4("projection", projection);
```

---

## 6. 立方体顶点数据

### 6.1 为何是 36 个顶点？

```
6 个面 × 2 个三角形/面 × 3 个顶点/三角形 = 36 个顶点
```

每个顶点 **8 个 float**：

```
位置 (x, y, z) + 颜色 (r, g, b) + 纹理坐标 (u, v)
```

立方体中心在原点，边长 1.0，局部坐标范围 `[-0.5, 0.5]`。

### 6.2 为何不用 EBO？

当前采用 **展开顶点 + `glDrawArrays`**：

```cpp
glDrawArrays(GL_TRIANGLES, 0, 36);
```

每个三角形的 3 个顶点在数组里**按顺序排列**，索引隐含在顺序中，不需要单独的 EBO。

同一角点在不同面上 **UV 可能不同**（例如贴图要铺满每个面），所以角点数据会重复出现——即使使用 EBO，通常也需要约 24 个独立顶点而非 8 个。教程为简化直接写 36 个展开顶点。

Day 03 矩形只有 4 个顶点、UV 可共享，用 EBO 更划算；立方体这里选 `glDrawArrays` 是为了降低理解成本。

---

## 7. 深度测试（3D 必须）

2D 矩形不需要区分前后；3D 立方体有面互相遮挡，必须开启：

```cpp
glEnable(GL_DEPTH_TEST);
```

每帧清屏时同时清除颜色缓冲和深度缓冲：

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

否则后面的面可能错误地画在前面面的上方。

---

## 8. 三个矩阵怎么定义？

### 8.1 View（循环外，固定相机）

```cpp
glm::mat4 view = glm::lookAt(
    glm::vec3(0.0f, 0.0f, 3.0f),   // 相机位置
    glm::vec3(0.0f, 0.0f, 0.0f),   // 看向原点（立方体中心）
    glm::vec3(0.0f, 1.0f, 0.0f)    // 上方向
);
```

相机不动时，View 每帧相同，放在循环外即可。

### 8.2 Projection（循环内，aspect 会变）

```cpp
float aspect = (float)g_ScreenWidth / (float)g_ScreenHeight;
glm::mat4 projection = glm::perspective(
    glm::radians(45.0f), aspect, 0.1f, 100.0f
);
```

| 参数 | 值 | 含义 |
|------|-----|------|
| FOV | 45° | 垂直视野 |
| aspect | 宽/高 | 防止窗口拉伸后物体变形 |
| near | 0.1 | 近裁剪面 |
| far | 100.0 | 远裁剪面 |

窗口 resize 时，`framebuffer_size_callback` 更新 `g_ScreenWidth/Height`，循环内重算 `projection`。

### 8.3 Model（循环内，每帧旋转）

```cpp
glm::mat4 model = glm::mat4(1.0f);   // 必须从单位矩阵开始
model = glm::rotate(model, glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));
```

| 步骤 | 作用 |
|------|------|
| 单位矩阵 | 变换起点，不可省略 |
| 绕 X 轴 -55° | 初始倾斜，方便观察 |
| 绕 `(0.5,1,0)` 随时间转 | 动态旋转动画 |

**Model 必须在循环内**：`glfwGetTime()` 每帧不同，矩阵每帧要重算（同 Day 05 原理）。

### 8.4 矩阵放循环内外的规则（复习）

| 矩阵 | 放哪 | 原因 |
|------|------|------|
| model | 循环内 | 随时间变化 |
| view | 循环外 | 相机固定 |
| projection | 循环内 | aspect 可能随 resize 变化 |

若以后加入 WASD 移动相机，则 `view` 也要移到循环内。

---

## 9. 窗口 resize 处理

```cpp
unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    g_ScreenWidth = (unsigned int)width;
    g_ScreenHeight = (unsigned int)height;
    glViewport(0, 0, width, height);
}
```

不更新 aspect 的话，拉伸窗口后立方体会被**压扁或拉长**。

---

## 10. 渲染循环核心步骤（每帧）

```
1. processInput          — ESC 退出
2. 计算 model            — 随时间旋转
3. 重算 projection       — 更新 aspect
4. glClear               — 颜色 + 深度
5. ourShader.use()
6. setMat4 × 3           — 传 MVP
7. 绑定纹理
8. glDrawArrays(36)      — 画立方体
9. SwapBuffers + PollEvents
```

---

## 11. 与 Day 06 数学文档的对应

| 代码 | 对应概念（见《坐标系统与空间变换链》） |
|------|----------------------------------------|
| `vertices[]` 局部坐标 | 局部空间（Model Space） |
| `glm::rotate` → model | Model 矩阵：局部 → 世界 |
| `glm::lookAt` → view | View 矩阵：世界 → 观察 |
| `glm::perspective` → projection | Projection 矩阵：观察 → 裁剪 |
| `gl_Position` 输出 | 裁剪空间 |
| GPU 自动透视除法 | 裁剪 → NDC |
| `glViewport` | NDC → 屏幕像素 |

投影矩阵内部数学见《投影矩阵数学原理》。

---

## 12. 常见问题

| 现象 | 可能原因 | 处理 |
|------|----------|------|
| 全黑 / 看不到立方体 | 相机位置或 near/far 不对 | 确认 `lookAt` 看向原点，near=0.1 |
| 画面闪烁、面互相穿透 | 未开深度测试或未清深度缓冲 | `glEnable(GL_DEPTH_TEST)` + `GL_DEPTH_BUFFER_BIT` |
| 旋转不动 | model 在循环外且只用一次 `glfwGetTime()` | model 移到循环内 |
| 窗口拉伸后变形 | aspect 未更新 | resize 回调里更新宽高，循环内重算 projection |
| Shader 改了没效果 | CMake 只复制一次 shaders | 重新 cmake 或手动复制到 build 目录 |
| 编译报错 `setMat4` | Shader.h 未包含 glm | 确认 `#include <glm/gtc/type_ptr.hpp>` |

---

## 13. Day 06 文档阅读顺序

```
① 坐标系统与空间变换链.md     → 理解五种空间、MVP 链
② 投影矩阵数学原理.md         → 理解 perspective 内部数学
③ 本文档                     → 对照 main.cpp 和 shader 读代码
```

---

## 14. 下一步可尝试

1. **阅读 Day 07** — `docs/Part1/Day07/摄像机系统概述与原理.md`（相机原理、FPS 控制、deltaTime）
2. **WASD 移动相机** — 每帧更新 `view`（`lookAt` 的 eye 位置）
3. **多个立方体** — 同一 VAO，循环里改不同的 `model` 再 `glDrawArrays`
4. **改用 EBO** — 24 顶点 + 36 索引，对比 VBO 大小
5. **正交投影对比** — 把 `glm::perspective` 换成 `glm::ortho` 看区别

---

## 15. 当前相关代码位置速查

| 内容 | 文件 | 大致位置 |
|------|------|----------|
| 立方体 36 顶点 | `main.cpp` | 第三阶段 vertices[] |
| 深度测试 | `main.cpp` | `glEnable(GL_DEPTH_TEST)` |
| View / Projection | `main.cpp` | 第三阶段末尾 |
| 每帧 Model 旋转 | `main.cpp` | 渲染循环开头 |
| MVP 上传 | `main.cpp` | `setMat4` × 3 |
| Shader MVP 声明 | `3.3.shader.vs` | uniform mat4 × 3 |
| setMat4 封装 | `Shader.h` | setMat4 方法 |
