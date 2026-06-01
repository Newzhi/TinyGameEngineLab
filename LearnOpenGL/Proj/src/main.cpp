#include <glad/glad.h>      // GLAD：负责加载 OpenGL 函数指针
#include <GLFW/glfw3.h>     // GLFW：负责创建窗口、处理输入与上下文

#include <iostream>         // 标准输入输出（这里用来打印错误信息）

// 输入处理函数：每一帧都调用一次
void processInput(GLFWwindow* window)
{
    // 若 ESC 被按下，则把窗口设置为“应关闭”
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// 窗口大小变化回调函数
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window; // 当前未使用该参数，显式标注避免部分编译器告警

    // 告诉 OpenGL：新的视口范围是整个窗口
    // 否则渲染区域可能仍按旧尺寸，导致显示异常
    glViewport(0, 0, width, height);
}


// 常量设置：窗口初始宽高（const 表示只读常量）
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

int main()
{
    // 1) 初始化 GLFW
    // glfwInit() 成功返回非 0，失败返回 0
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // 2) 配置 OpenGL 版本与模式
    // 这里要求创建 OpenGL 3.3 Core Profile 上下文
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    // macOS 需要这个前向兼容选项
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // 3) 创建窗口（并隐式创建 OpenGL 上下文）
    // 参数：宽, 高, 标题, 全屏显示器(这里为 nullptr), 共享上下文(这里为 nullptr)
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 4) 把当前线程绑定到这个 OpenGL 上下文
    glfwMakeContextCurrent(window);

    // 5) 注册窗口尺寸变化回调：窗口大小变化时会自动调用该函数
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 6) 通过 GLFW 提供的地址解析函数初始化 GLAD
    // 没有这一步，OpenGL API 函数指针大多不可用
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 7) 渲染循环：只要窗口没有被标记为关闭，就持续渲染
    while (!glfwWindowShouldClose(window))
    {
        // 7.1 处理输入（例如按 ESC 退出）
        processInput(window);

        // 7.2 渲染：先设置清屏颜色，再清空颜色缓冲
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);   // RGBA，范围 [0, 1]
        glClear(GL_COLOR_BUFFER_BIT);

        // 7.3 交换前后缓冲（把本帧显示出来）+ 轮询输入事件
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 8) 释放 GLFW 相关资源
    glfwTerminate();
    return 0;
}

