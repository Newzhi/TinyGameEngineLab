# 鼠标回调与 FPS 摄像机代码实践（基于当前 `main.cpp`）

这份文档对应 Day 07 的**代码实践**，配合《摄像机系统概述与原理》，对照 `main.cpp` 讲解 **GLFW 回调、WASD、鼠标环视、滚轮缩放** 的完整逻辑。

| 文档 | 内容 |
|------|------|
| 《摄像机系统概述与原理》 | 相机概念、yaw/pitch、deltaTime |
| **本文档** | 回调注册、全局状态、每帧数据流、练习（含右键按住才环视） |
| 《MVP 立方体与代码实践》（Day 06） | MVP 与立方体绘制基础 |

---

## 1. Day 06 → Day 07：多了什么？

Day 06 相机固定，`view` 写死或自动轨道。Day 07 在 Day 06 立方体场景上增加：

| 新增 | 作用 |
|------|------|
| 全局相机变量 | `cameraPos`、`cameraFront`、`yaw`、`pitch`、`fov` |
| `mouse_callback` | 鼠标移动 → 更新朝向 |
| `scroll_callback` | 滚轮 → 更新 FOV |
| `processInput` 扩展 | WASD → 平移 `cameraPos` |
| `deltaTime` | 移动速度不随 FPS 变化 |
| `GLFW_CURSOR_DISABLED` | FPS 模式，隐藏并锁定光标 |
| 回调注册 | `glfwSetCursorPosCallback` / `glfwSetScrollCallback` |

Shader **不用改**，仍接收 `uniform mat4 view`。

---

## 2. 整体数据流（一帧内）

```
glfwPollEvents()
    ↓
  （GLFW 自动触发已注册的回调）
    ├── mouse_callback   → 改 yaw/pitch → cameraFront
    └── scroll_callback  → 改 fov

渲染循环每帧：
    ├── 算 deltaTime
    ├── processInput     → WASD 改 cameraPos
    ├── view = lookAt(cameraPos, cameraPos + cameraFront, cameraUp)
    ├── projection = perspective(fov, aspect, ...)
    └── setMat4("view", view)  → 绘制立方体
```

**关键**：回调改「相机状态」，渲染循环用状态算 `view` / `projection`，再上传 GPU。

---

## 3. 全局变量（为什么放文件顶部？）

```cpp
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

float yaw   = -90.0f;   // 初始朝 -Z，与 cameraFront 一致
float pitch = 0.0f;

bool  firstMouse = true;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;

float fov = 45.0f;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
```

| 变量 | 谁写 | 谁读 |
|------|------|------|
| `cameraPos` | `processInput`（WASD） | 渲染循环 `lookAt` |
| `cameraFront` | `mouse_callback` | 渲染循环 `lookAt`、`processInput` |
| `yaw` / `pitch` | `mouse_callback` | `mouse_callback` 内算 front |
| `fov` | `scroll_callback` | 渲染循环 `perspective` |
| `deltaTime` | 渲染循环开头 | `processInput` |

回调函数签名由 GLFW 规定，**不能随意加参数**，所以相机状态通常用**全局变量**或「`glfwSetWindowUserPointer` + 结构体」传递。入门阶段用全局变量最简单。

---

## 4. 注册回调（main 里，窗口创建之后）

```cpp
glfwMakeContextCurrent(window);
glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

// 鼠标移动、滚轮
glfwSetCursorPosCallback(window, mouse_callback);
glfwSetScrollCallback(window, scroll_callback);

// FPS 模式：光标隐藏并锁定在窗口中心
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
```

| API | 触发时机 |
|-----|----------|
| `glfwSetCursorPosCallback` | 鼠标在窗口内移动 |
| `glfwSetScrollCallback` | 滚轮滚动 |
| `GLFW_CURSOR_DISABLED` | 光标不可见，移出窗口会被拉回 |

回调在 `glfwPollEvents()` 时被调用，**不要在渲染循环里手动调用** `mouse_callback`。

---

## 5. `mouse_callback` 逻辑（逐步）

```cpp
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // ① 首帧：只记录位置，跳过偏移计算
    if (firstMouse) {
        lastX = xpos; lastY = ypos;
        firstMouse = false;
        return;  // 或直接不设 return，但 offset 应为 0
    }

    // ② 算相对上一帧的位移
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;   // Y 轴反向：鼠标上抬 → pitch 增大

    lastX = xpos;
    lastY = ypos;

    // ③ 灵敏度缩放
    float sensitivity = 0.05f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // ④ 更新欧拉角
    yaw   += xoffset;
    pitch += yoffset;
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // ⑤ yaw/pitch → 前向向量
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}
```

### 5.1 为什么要 `firstMouse`？

启用 `GLFW_CURSOR_DISABLED` 后，第一帧光标可能从屏幕任意位置「跳」到窗口中心。若直接算 `xoffset`，会出现**视角猛转一下**。首帧只同步 `lastX/lastY`，从第二帧才开始算偏移。

### 5.2 为什么 `yoffset = lastY - ypos`？

屏幕坐标 Y 向下增大，而「鼠标向上抬」应让视角上仰（pitch 增大），所以要**反过来减**。

### 5.3 为什么限制 pitch ±89°？

pitch 到 ±90° 时 `front` 与 `worldUp` 平行，叉积不稳定（万向锁）。限制在 ±89° 内可避免。

---

## 6. `scroll_callback` 逻辑

```cpp
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    fov -= yoffset;
    if (fov < 1.0f)  fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}
```

| 行为 | 改什么 | 视觉效果 |
|------|--------|----------|
| 滚轮 | `fov`（projection） | 拉近/拉远视野，**相机位置不变** |
| WASD | `cameraPos`（view） | 人在场景里走 |
| 鼠标 | `cameraFront`（view） | 转头环视 |

---

## 7. `processInput`：WASD + deltaTime

```cpp
void processInput(GLFWwindow *window)
{
    float cameraSpeed = 2.5f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;

    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= right * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += right * cameraSpeed;
}
```

渲染循环开头：

```cpp
float currentFrame = glfwGetTime();
deltaTime = currentFrame - lastFrame;
lastFrame = currentFrame;
processInput(window);
```

`2.5f * deltaTime` 表示**每秒移动 2.5 单位**，与帧率无关。

---

## 8. 渲染循环里如何用相机状态

```cpp
glm::mat4 view = glm::lookAt(
    cameraPos,
    cameraPos + cameraFront,   // 看向前方 1 单位处，不是固定原点
    cameraUp
);

float aspect = (float)g_ScreenWidth / (float)g_ScreenHeight;
glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);

ourShader.setMat4("view", view);
ourShader.setMat4("projection", projection);
```

与 Day 06 的区别：

| | Day 06 | Day 07 |
|---|--------|--------|
| `lookAt` 目标 | 固定 `(0,0,0)` | `cameraPos + cameraFront` |
| `view` 更新 | 可放循环外 | **必须每帧**（输入会变） |
| `fov` | 固定 45° | 滚轮可改 |

---

## 9. 回调 vs 轮询：两种输入方式

| 方式 | API 示例 | 适用 |
|------|----------|------|
| **轮询** | `glfwGetKey(window, KEY_W)` | 键盘按住持续生效 |
| **回调** | `glfwSetCursorPosCallback` | 鼠标移动、滚轮（事件驱动） |

键盘用轮询（每帧问「W 是否按下」）；鼠标移动用回调（只有动了才通知，且能拿到精确坐标）。

鼠标**按键**（左/右/中）可以用：

- 轮询：`glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)`
- 回调：`glfwSetMouseButtonCallback`

---

## 10. 练习：按住右键才允许环视（重点）

当前代码使用 `GLFW_CURSOR_DISABLED`，**一进入窗口鼠标就控制视角**。若希望：

- 平时：光标可见，可点 UI / 操作系统
- **按住右键时**：才环视，类似 Blender / 很多 3D 编辑器

### 10.1 思路

```
不按右键 → 光标正常（NORMAL），mouse_callback 直接 return
按住右键 → 光标隐藏锁定（DISABLED），处理 yaw/pitch
松开右键 → 恢复 NORMAL，重置 firstMouse
```

### 10.2 新增全局变量

```cpp
bool rightMousePressed = false;
```

### 10.3 注册鼠标按键回调

在 `main` 里与另外两个回调一起注册：

```cpp
glfwSetMouseButtonCallback(window, mouse_button_callback);
```

**不要**在初始化时使用 `GLFW_CURSOR_DISABLED`，改为正常光标：

```cpp
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
```

### 10.4 `mouse_button_callback` 实现

```cpp
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    (void)mods;

    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            rightMousePressed = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;   // 按下右键后下一帧重新同步，避免跳变
        }
        else if (action == GLFW_RELEASE)
        {
            rightMousePressed = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}
```

### 10.5 修改 `mouse_callback` 开头

```cpp
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    (void)window;

    // 未按住右键时不更新视角
    if (!rightMousePressed)
        return;

    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    // ... 后面 yaw/pitch 逻辑不变 ...
}
```

### 10.6 流程图

```
用户按下右键
    → mouse_button_callback(PRESS)
    → rightMousePressed = true
    → GLFW_CURSOR_DISABLED
    → firstMouse = true

用户移动鼠标
    → mouse_callback
    → rightMousePressed == true → 更新 yaw/pitch

用户松开右键
    → mouse_button_callback(RELEASE)
    → rightMousePressed = false
    → GLFW_CURSOR_NORMAL（光标恢复）
```

### 10.7 可选扩展

| 需求 | 做法 |
|------|------|
| 右键按住时 WASD 才生效 | 在 `processInput` 开头加 `if (!rightMousePressed) return;` |
| 滚轮仅右键按住时缩放 | 在 `scroll_callback` 开头同样判断 |
| 左键点击选中物体 | `GLFW_MOUSE_BUTTON_LEFT` + 射线检测（后续章节） |

---

## 11. 练习：按住右键 + WASD 才移动（组合）

若希望 **WASD 也只在按住右键时生效**（完全「右键拖拽模式」）：

```cpp
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (!rightMousePressed)
        return;

    float cameraSpeed = 2.5f * deltaTime;
    // ... WASD 逻辑不变 ...
}
```

这样不按右键时，键盘和鼠标都不会动相机，适合编辑器式操作。

---

## 12. 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 一进窗口视角猛转 | 缺 `firstMouse` | 首帧只记录 lastX/lastY |
| 鼠标上下反了 | yoffset 方向错 | 用 `lastY - ypos` |
| WASD 速度随 FPS 变 | 没用 deltaTime | `speed * deltaTime` |
| 鼠标不动视角也在抖 | 灵敏度太大 | 降低 `sensitivity` |
| 滚轮无效 | 未注册 scroll 回调 | `glfwSetScrollCallback` |
| 右键模式一按就跳 | 未在 PRESS 时设 `firstMouse=true` | 见 10.4 |
| 松开后光标不出现 | 未设 `GLFW_CURSOR_NORMAL` | 在 RELEASE 里恢复 |

---

## 13. 小练习

### 基础

1. 把 `sensitivity` 改成 `0.1f`，环视变快还是变慢？
2. 注释掉 `pitch` 的 ±89° 限制，垂直转到顶时画面有何异常？
3. `fov` 改成 90° 和 20°，立方体大小如何变化？
4. 画出：`mouse_callback` 改了哪些变量？渲染循环里谁消费这些变量？

### 进阶（建议动手改 `main.cpp`）

5. **实现 10 节**：按住右键才环视，松开恢复光标。
6. 在练习 5 基础上，WASD 也仅在按住右键时生效。
7. 增加 **Shift** 键：按住时 `cameraSpeed * 2` 加速。
8. 按 **R** 键重置相机到 `pos=(0,0,3)`、`yaw=-90`、`pitch=0`。
9. （可选）封装 `Camera` 类，把全局变量收进类成员。

### 思考

10. 为什么滚轮改 `fov` 而不是改 `cameraPos.z`？两种方式观感有何不同？
11. `mouse_callback` 和 `processInput` 的执行顺序取决于什么？如果在 `processInput` 之后才算 `view` 会有问题吗？

---

## 14. 当前相关文件

```
Proj/
  src/cppfile/main.cpp
    ├── 全局相机变量
    ├── glfwSetCursorPosCallback / glfwSetScrollCallback
    ├── mouse_callback / scroll_callback / processInput
    └── 渲染循环：lookAt + perspective(fov)
  shaders/3.3.shader.vs          ← uniform mat4 view（未改）
  docs/Part1-Base/Day07/
    摄像机系统概述与原理.md
    鼠标回调与FPS摄像机代码实践.md   ← 本文
```

---

## 15. 阅读顺序

```
Day07 摄像机系统概述与原理.md     → 概念
Day07 鼠标回调与FPS摄像机代码实践.md → 本文，对照 main.cpp
        ↓
动手完成练习 5~8（右键按住环视）
        ↓
（可选）封装 Camera 类
```

---

## 16. 参考文献

1. LearnOpenGL CN — [摄像机](https://learnopengl-cn.github.io/01%20Getting%20started/09%20Camera/)
2. GLFW 文档 — [Input guide](https://www.glfw.org/docs/latest/input_guide.html)（回调、光标模式、鼠标按钮）
3. 《Day 07 — 摄像机系统概述与原理》— yaw/pitch、deltaTime 概念
