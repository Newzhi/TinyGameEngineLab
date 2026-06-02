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

// 编译单个 Shader（顶点/片段都可复用）
// type: GL_VERTEX_SHADER 或 GL_FRAGMENT_SHADER
// source: GLSL 源码字符串
unsigned int compileShader(unsigned int type, const char* source)
{
    // 1) 创建 Shader 对象，返回其句柄（类似“资源 ID”）
    unsigned int shader = glCreateShader(type);

    // 2) 把 GLSL 源码交给 OpenGL
    // 参数含义：
    // - shader: 目标 Shader 对象
    // - 1: 传入源码字符串数量（这里 1 段）
    // - &source: 源码地址
    // - nullptr: 让驱动自己按 \0 结尾判断长度
    glShaderSource(shader, 1, &source, nullptr);

    // 3) 执行编译（CPU 提交，驱动在内部完成编译流程）
    glCompileShader(shader);

    // 4) 查询编译结果（GL_TRUE / GL_FALSE）
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        // 5) 编译失败时读取日志，帮助定位具体哪一行 GLSL 出错
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;

        // 这里没有直接退出程序，便于教学演示；
        // 实战里通常会进一步中止流程或抛异常。
    }

    // 返回 Shader 句柄，供后续 attach + link 使用
    return shader;
}

// 链接着色器程序，失败时打印链接错误日志
// 一个 program 至少要有：顶点着色器 + 片段着色器
unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource)
{
    // 1) 先分别编译两个阶段的 Shader
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    // 2) 创建 Program 对象（可理解为“可执行的 GPU 渲染程序”容器）
    unsigned int shaderProgram = glCreateProgram();

    // 3) 把已编译的 Shader 挂到 Program 上
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    // 4) 执行链接：把各阶段接口（如 out/in）匹配并组合为可用程序
    glLinkProgram(shaderProgram);

    // 5) 查询链接状态
    int success = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        // 6) 读取 Program 级别日志（常见于阶段间变量不匹配）
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM_LINKING_FAILED\n" << infoLog << std::endl;
    }

    // 7) 链接完成后即可删除独立 Shader 对象
    // 因为 Program 内部已经持有可执行结果，不再依赖这两个临时对象。
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 返回可供 glUseProgram 使用的 Program 句柄
    return shaderProgram;
}


// 常量设置：窗口初始宽高（const 表示只读常量）
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

int main()
{
    // =========================
    // CPU 侧准备阶段（还未进入 GPU 绘制）
    // =========================
    // Shader 源码就是普通字符串，放在这里定义即可；真正生效要等后面编译/链接。
    const char* vertexShaderSource = "#version 330 core\n"
        // location=0：告诉 GPU，这个输入变量从“顶点属性槽 0”读取数据。
        "layout (location = 0) in vec3 aPos;\n"
        "void main()\n"
        "{\n"
        // gl_Position 是顶点着色器必须写出的裁剪空间坐标（clip space）。
        // 本例直接把输入坐标原样输出，w=1.0。
        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "}\0";

    const char* fragmentShaderSource = "#version 330 core\n"
        // 片段着色器输出颜色（最终写入颜色缓冲）
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        // RGBA：橙色
        "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
        "}\0";

    // 顶点数据说明：
    // 下面先保留“画单个三角形”的旧写法（已注释），便于对照学习；不删除历史代码。
    // 这些坐标在 NDC（标准化设备坐标）范围内，x/y/z 通常在 [-1, 1] 可见。
    // float vertices[] = {
    //     -0.5f, -0.5f, 0.0f,
    //      0.5f, -0.5f, 0.0f,
    //      0.0f,  0.5f, 0.0f
    // };

    // 当前启用的是“4 顶点 + 索引”的矩形写法：
    // 只存 4 个角点，再用 indices 指定如何拼成 2 个三角形。
    float vertices[] = {
        0.5f, 0.5f, 0.0f,   // 0: 右上角
        0.5f, -0.5f, 0.0f,  // 1: 右下角
        -0.5f, -0.5f, 0.0f, // 2: 左下角
        -0.5f, 0.5f, 0.0f   // 3: 左上角
    };

    // EBO 索引：告诉 GPU 按什么顺序取 vertices 里的顶点。
    // 注意索引从 0 开始，索引值就是 vertices 的“顶点编号”。
    unsigned int indices[] = {
        0, 1, 3, // 第一个三角形（右上、右下、左上）
        1, 2, 3  // 第二个三角形（右下、左下、左上）
    };

    // 1) 初始化 GLFW
    // 作用：初始化窗口系统与上下文相关资源。失败通常是环境/驱动问题。
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // 2) 配置 OpenGL 上下文属性（在创建窗口前设置）
    // 请求 3.3 Core Profile：只使用现代 OpenGL API。
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    // macOS 需要 forward compatibility 才能拿到可用的 3.3+ core context。
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // 3) 创建窗口（同时创建 OpenGL Context）
    // 参数：宽、高、标题、全屏监视器(这里 nullptr=窗口模式)、共享上下文(这里不共享)。
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 4) 让当前线程“拥有”这个上下文
    // 很多 OpenGL 调用都依赖当前线程存在有效 context。
    glfwMakeContextCurrent(window);

    // 5) 注册帧缓冲尺寸回调
    // 窗口大小变化时，更新 glViewport，避免拉伸或裁剪异常。
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 6) 初始化 GLAD（加载 OpenGL 函数指针）
    // 不做这一步，像 glCreateShader/glGenBuffers 等函数指针可能是空的。
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 7) 编译并链接着色器程序
    // shaderProgram 可以理解为“一套 GPU 流水线阶段配置”（此例仅顶点+片段）。
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // 8) 创建 VAO / VBO / EBO，并把数据上传到 GPU
    // - VBO (Vertex Buffer Object): 存顶点属性原始数据
    // - EBO (Element Buffer Object): 存索引数据（告诉 GPU 顶点复用关系）
    // - VAO (Vertex Array Object): 记录“顶点属性布局 + 相关绑定状态”
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO); // 生成 1 个 VAO 对象句柄
    glGenBuffers(1, &VBO);      // 生成 1 个 VBO 对象句柄
    glGenBuffers(1, &EBO);      // 生成 1 个 EBO 对象句柄

    // 推荐顺序：先绑定 VAO，再绑 VBO/EBO，再配置顶点属性。
    // 这样这些状态会一起被 VAO 记住，后续只要 glBindVertexArray(VAO) 即可恢复。
    glBindVertexArray(VAO);

    // 绑定 VBO 并上传顶点数据
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 绑定 EBO 并上传索引数据
    // 注意：EBO 的绑定属于 VAO 状态的一部分，所以通常在 VAO 绑定期间设置。
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


    // 告诉 OpenGL 如何从当前绑定的 VBO 读取“顶点属性 0”
    // 参数解释：
    // 0  -> 对应 layout(location=0)
    // 3  -> 每个顶点该属性有 3 个分量 (x,y,z)
    // GL_FLOAT -> 分量类型
    // GL_FALSE -> 不做归一化（float 数据通常就是 FALSE）
    // stride = 3*sizeof(float) -> 相邻顶点在内存中的步长
    // offset = 0 -> 从当前顶点起始位置读取
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // 启用顶点属性槽 0。只定义不启用的话，绘制时不会读取该属性。
    glEnableVertexAttribArray(0);

    // 可选：解绑 VAO，避免后续误改状态。
    glBindVertexArray(0);

    // 可视化调试：线框模式（只画边，不填充）
    // 便于观察三角形拼接关系；想恢复实体填充可改为 GL_FILL。
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // 9) 渲染循环（每帧执行）
    while (!glfwWindowShouldClose(window))
    {
        // 9.1 处理输入（ESC 关闭窗口）
        processInput(window);

        // 9.2 清屏：先设置清屏色，再清空颜色缓冲
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 9.3 选择要使用的着色器程序
        glUseProgram(shaderProgram);

        // 9.4 绑定包含顶点格式状态的 VAO
        glBindVertexArray(VAO);

        // 9.5 发起绘制（二选一，保留注释代码方便对照学习）
        // A. 非索引绘制：顺序取顶点数组（这里保留你之前三角形练习代码）
        // glDrawArrays(GL_TRIANGLES, 0, 3);

        // B. 索引绘制：按 EBO 中的索引取顶点，支持顶点复用
        // 6 = 本例索引总数（两个三角形共 6 个索引）
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // 9.6 双缓冲交换：把后台缓冲显示到前台，避免闪烁/撕裂
        glfwSwapBuffers(window);

        // 9.7 处理窗口事件队列（键盘、鼠标、窗口尺寸变化等）
        glfwPollEvents();
    }


    // 10) 释放 OpenGL 与 GLFW 资源
    // 与创建顺序对应释放，便于维护与排查泄漏。
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}