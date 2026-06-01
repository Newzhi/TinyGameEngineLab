# 常用 API 与窗口渲染流程说明（基于当前 `main.cpp`）

这份文档对应你的 `Proj/src/main.cpp`，目标是让你知道三件事：

1. 常用 API 都在做什么
2. 为什么要按现在这种顺序写
3. 从“启动程序”到“窗口显示出来”到底发生了什么

---

## 1. 先看整体：窗口显示的完整链路

你的代码可以理解为 8 个阶段：

1. **初始化 GLFW**：`glfwInit()`
2. **声明要什么版本的 OpenGL**：`glfwWindowHint(...)`
3. **创建窗口 + 上下文**：`glfwCreateWindow(...)`
4. **绑定上下文到当前线程**：`glfwMakeContextCurrent(window)`
5. **注册窗口大小变化回调**：`glfwSetFramebufferSizeCallback(...)`
6. **初始化 GLAD，加载 OpenGL 函数地址**：`gladLoadGLLoader(...)`
7. **进入渲染循环**：输入处理 + 清屏 + 交换缓冲 + 事件轮询
8. **程序退出时释放资源**：`glfwTerminate()`

> 这 8 步里，最容易漏掉的是第 4、6 步。漏了就会出现“函数不可用”“黑屏”或直接崩溃。

---

## 2. 常用 API 速查（作用 + 为什么这样写）

## 2.1 GLFW 相关

- `glfwInit()`
  - **作用**：初始化 GLFW 库。
  - **为什么**：不初始化就不能创建窗口、处理输入、管理上下文。

- `glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3)`
- `glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3)`
- `glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE)`
  - **作用**：告诉 GLFW 你要 OpenGL 3.3 Core Profile。
  - **为什么**：不同机器支持的默认上下文不同，明确指定能减少兼容问题。

- `glfwCreateWindow(width, height, title, nullptr, nullptr)`
  - **作用**：创建窗口，同时创建 OpenGL 上下文。
  - **为什么**：OpenGL 的所有渲染操作都依赖上下文，没有它就无法绘制。

- `glfwMakeContextCurrent(window)`
  - **作用**：把窗口对应的 OpenGL 上下文绑定到当前线程。
  - **为什么**：OpenGL API 是“对当前上下文生效”的；不绑定就不知道往哪个窗口画。

- `glfwSetFramebufferSizeCallback(window, framebuffer_size_callback)`
  - **作用**：窗口像素大小变化时自动调用回调。
  - **为什么**：高 DPI 或用户拉伸窗口时，渲染区域会变化，不更新视口就会显示异常。

- `glfwGetKey(window, GLFW_KEY_ESCAPE)`
  - **作用**：读取按键状态。
  - **为什么**：在渲染循环中轮询输入是最常见的新手入口。

- `glfwSetWindowShouldClose(window, true)`
  - **作用**：标记窗口应关闭。
  - **为什么**：与循环条件 `glfwWindowShouldClose(window)` 配合，优雅退出。

- `glfwSwapBuffers(window)`
  - **作用**：交换前后缓冲，把本帧结果显示到屏幕。
  - **为什么**：双缓冲避免画面撕裂/闪烁。

- `glfwPollEvents()`
  - **作用**：处理系统事件（键盘、鼠标、窗口拖拽等）。
  - **为什么**：不轮询事件，窗口可能“卡死无响应”。

- `glfwTerminate()`
  - **作用**：释放 GLFW 相关资源。
  - **为什么**：程序结束前做清理，避免资源泄漏；失败分支也要调用它。

## 2.2 GLAD / OpenGL 相关

- `gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)`
  - **作用**：根据当前上下文动态加载 OpenGL 函数指针。
  - **为什么**：Windows 等平台下，很多 OpenGL 函数在编译期不可直接链接，必须运行时拿地址。

- `glViewport(0, 0, width, height)`
  - **作用**：设置 OpenGL 最终输出到窗口的像素区域。
  - **为什么**：窗口尺寸改变后若不更新，渲染会被拉伸、裁剪或比例不对。

- `glClearColor(r, g, b, a)`
  - **作用**：设置“清屏颜色”。
  - **为什么**：下一次 `glClear(GL_COLOR_BUFFER_BIT)` 会用这个颜色填满颜色缓冲。

- `glClear(GL_COLOR_BUFFER_BIT)`
  - **作用**：清空颜色缓冲。
  - **为什么**：每帧开始先“擦黑板”，避免上一帧残影。

---

## 3. 为什么调用顺序基本不能乱

一个关键原则：

- **先有上下文**（`glfwCreateWindow` + `glfwMakeContextCurrent`）
- **再加载函数**（`gladLoadGLLoader`）
- **再调用 OpenGL API**（比如 `glClearColor`、`glClear`）

如果你先调用 OpenGL API 再初始化 GLAD，常见结果是函数指针为空，程序异常。

---

## 4. “画出窗口”时，底层做了哪些事（概念版）

当你运行程序后，系统大致经历：

1. GLFW 向操作系统申请窗口资源（窗口句柄、消息队列等）
2. GLFW 创建并管理 OpenGL 上下文（驱动层对象）
3. GLAD 读取驱动暴露的函数地址，填到对应函数指针
4. 渲染循环里：
   - 处理输入和系统消息
   - CPU 发 OpenGL 指令（如清屏）
   - GPU 执行并写入后缓冲
   - 交换缓冲，显示到屏幕

你看到“窗口变成指定背景色”的本质，就是 GPU 把颜色缓冲填充后，通过交换缓冲展示出来。

---

## 5. 案例演示

## 5.1 案例 A：最小开窗 + 青色背景

目标：打开窗口并持续显示背景色。

关键语句：

- 初始化与创建：`glfwInit`、`glfwCreateWindow`
- 绑定与加载：`glfwMakeContextCurrent`、`gladLoadGLLoader`
- 每帧渲染：`glClearColor` + `glClear`
- 刷新显示：`glfwSwapBuffers` + `glfwPollEvents`

你当前 `main.cpp` 已经完整覆盖这个案例。

## 5.2 案例 B：ESC 退出 + 自适应窗口尺寸

目标：让程序更“像一个可交互应用”。

关键语句：

- ESC 退出：
  - `glfwGetKey(window, GLFW_KEY_ESCAPE)`
  - `glfwSetWindowShouldClose(window, true)`
- 尺寸变化：
  - `glfwSetFramebufferSizeCallback(...)`
  - 回调内 `glViewport(0, 0, width, height)`

你当前 `main.cpp` 也已经实现了这两点，是非常标准的 LearnOpenGL 入门结构。

---

## 6. 常见问题（你现在就可能遇到）

- **黑屏/崩溃**：通常是 GLAD 初始化失败，或者在 GLAD 之前调用了 OpenGL API。
- **窗口能开但卡住**：通常忘了 `glfwPollEvents()`。
- **拉伸窗口后画面不对**：通常忘了在回调里调用 `glViewport`。
- **退出分支写 `glfwTerminate()` 是什么意思？**
  - 就是“清理 GLFW 全局资源后再退出程序”，属于良好收尾动作。

---

## 7. 下一步建议（从“能开窗”到“能画图元”）

你下一阶段可以按这个顺序继续：

1. 创建并编译 Shader（顶点着色器 + 片段着色器）
2. 建立 VAO/VBO，上传三角形顶点
3. 在循环中 `glDrawArrays` 画出第一个三角形

做到这一步，你就从“窗口清屏”跨到“真正绘制几何体”了。