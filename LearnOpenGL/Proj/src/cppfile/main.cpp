// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
// GLFW：创建窗口、处理输入、管理 OpenGL 上下文
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"

#include <iostream>

// 前向声明：函数定义在文件末尾
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// 常量设置：窗口初始宽高（NDC 坐标与窗口像素尺寸无关，这里只决定窗口大小）
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

int main()
{
    // =========================
    // 第一阶段：初始化 GLFW + 创建窗口 + 加载 OpenGL
    // =========================

    // glfwInit：初始化 GLFW 库，是使用 GLFW 任何功能的前置步骤
    glfwInit();

    // glfwWindowHint：在创建窗口「之前」设置 OpenGL 上下文属性
    // 告诉 GLFW 我们要 OpenGL 3.3 版本、Core Profile（现代 OpenGL，不含废弃 API）
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    // macOS 需要开启 forward compatibility 才能创建 3.3+ core context
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfwCreateWindow：创建窗口，同时创建关联的 OpenGL 上下文
    // 返回值 GLFWwindow* 是后续所有 GLFW 操作的句柄
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // glfwMakeContextCurrent：把该窗口的 OpenGL 上下文绑定到当前线程
    // OpenGL 是「线程 + 上下文」绑定的，不绑定就无法调用 OpenGL API
    glfwMakeContextCurrent(window);

    // glfwSetFramebufferSizeCallback：注册窗口尺寸变化回调
    // 用户拖动窗口边缘或系统缩放时，会自动调用 framebuffer_size_callback
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // gladLoadGLLoader：加载 OpenGL 函数指针（必须在有效上下文创建之后）
    // glfwGetProcAddress 是 GLFW 提供的「查函数地址」接口，交给 GLAD 使用
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // =========================
    // 第二阶段：编译链接着色器（Shader 类内部完成）
    // =========================

    // Shader 构造函数会：读取 .vs/.fs 文件 → 编译 → 链接 → 得到可用的 GPU 程序
    // 路径是运行时相对路径，CMake 会把 shaders/ 复制到构建输出目录
    Shader ourShader("shaders/3.3.shader.vs", "shaders/3.3.shader.fs");

    // =========================
    // 第三阶段：准备顶点数据，上传到 GPU
    // =========================

    // 顶点数组：每个顶点 6 个 float = 位置(x,y,z) + 颜色(r,g,b)
    // 坐标使用 NDC（标准化设备坐标），范围约 [-1, 1]，(0,0) 在屏幕中心
    float vertices[] = {
        // 位置(x,y,z)        颜色(r,g,b)
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // 顶点0：右下，红
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // 顶点1：左下，绿
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // 顶点2：顶部，蓝
    };

    // VAO（Vertex Array Object）：记录「顶点数据怎么读」的配置快照
    // VBO（Vertex Buffer Object）：在 GPU 显存里存顶点原始数据
    unsigned int VBO, VAO;

    // glGenVertexArrays：向 GPU 申请 1 个 VAO，id 写入 VAO
    glGenVertexArrays(1, &VAO);
    // glGenBuffers：向 GPU 申请 1 个 Buffer，id 写入 VBO
    glGenBuffers(1, &VBO);

    // glBindVertexArray：绑定 VAO，之后对 VBO/顶点属性的配置都会记录在这个 VAO 上
    // 推荐顺序：先绑 VAO → 再绑 VBO → 上传数据 → 配置属性
    glBindVertexArray(VAO);

    // glBindBuffer(GL_ARRAY_BUFFER, VBO)：
    // 把 VBO 绑定到 GL_ARRAY_BUFFER 目标（表示这块 buffer 用来存顶点数组）
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // glBufferData：分配 GPU 显存，并把 CPU 的 vertices 数据拷贝进去
    // 参数：目标 | 字节数 | 数据指针 | 使用方式 hint
    // GL_STATIC_DRAW：数据基本不变，只用来绘制（适合固定几何体）
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // --- 配置顶点属性 0：位置 aPos（对应 shader 里 layout(location=0) in vec3 aPos）---
    // glVertexAttribPointer 告诉 GPU 如何从 VBO 里解析这个属性：
    //   index=0        → 对应 location 0
    //   size=3         → 每个顶点该属性 3 个分量 (x,y,z)
    //   type=GL_FLOAT  → 分量类型是 float
    //   normalized=false → 不做 [0,255]→[0,1] 归一化
    //   stride=6*sizeof(float) → 相邻两个顶点间隔 6 个 float（位置3 + 颜色3）
    //   offset=0       → 从每个顶点块的开头读位置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // glEnableVertexAttribArray：启用 location 0 这条顶点属性通道
    glEnableVertexAttribArray(0);

    // --- 配置顶点属性 1：颜色 aColor（对应 shader 里 layout(location=1) in vec3 aColor）---
    // offset = 3*sizeof(float)：跳过前 3 个 float（位置），从颜色开始读
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 可选：glBindVertexArray(0) 解绑 VAO，防止后续误改
    // 实际项目中常不解绑，因为改其他 VAO 时本来就要先 bind

    // =========================
    // 第四阶段：渲染循环（每帧重复）
    // =========================
    while (!glfwWindowShouldClose(window))
    {
        // 处理键盘输入（如 ESC 关闭窗口）
        processInput(window);

        // glClearColor：设置清屏用的背景色 (R, G, B, A)，范围 0.0~1.0
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        // glClear：清空颜色缓冲（相当于每帧开始前「擦黑板」）
        // GL_COLOR_BUFFER_BIT 表示只清颜色，不清深度/模板缓冲
        glClear(GL_COLOR_BUFFER_BIT);

        // ourShader.use() → 内部调用 glUseProgram(ID)，告诉 GPU 用哪套着色器程序
        ourShader.use();
        // glBindVertexArray：恢复之前记录好的 VBO 绑定 + 顶点属性配置
        glBindVertexArray(VAO);
        // glDrawArrays：发起绘制调用（Draw Call）
        //   GL_TRIANGLES → 每 3 个顶点拼成一个三角形
        //   0            → 从第 0 号顶点开始
        //   3            → 一共取 3 个顶点（1 个三角形）
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // glfwSwapBuffers：双缓冲交换——把后台画好的帧显示到屏幕，避免闪烁
        // glfwPollEvents：处理窗口事件队列（键盘、鼠标、窗口大小变化等）
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // =========================
    // 第五阶段：释放 GPU 资源
    // =========================
    // glDeleteVertexArrays / glDeleteBuffers：释放 GPU 上的 VAO、VBO 对象
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    // glfwTerminate：销毁所有 GLFW 资源，程序退出前调用
    glfwTerminate();
    return 0;
}

// 每帧调用一次：查询按键状态并响应
void processInput(GLFWwindow *window)
{
    // glfwGetKey：查询某键当前是否按下；GLFW_PRESS 表示「本帧处于按下状态」
    // glfwSetWindowShouldClose：标记窗口应关闭，渲染循环会在下一帧退出
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// 窗口 framebuffer 尺寸变化时由 GLFW 自动调用
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window; // 当前未使用，避免编译器警告

    // glViewport：设置 OpenGL 渲染区域（视口）
    // (0,0) 是窗口左下角，width/height 是像素尺寸
    // 窗口被拉伸时必须更新，否则画面只画在旧尺寸区域
    glViewport(0, 0, width, height);
}
