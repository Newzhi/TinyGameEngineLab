// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
// GLFW：创建窗口、处理输入、管理 OpenGL 上下文
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// stb 单头文件库：整个项目只在此处定义一次实现
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// 窗口尺寸（resize 时更新，用于计算 projection 的 aspect 比例）
unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

int main()
{
    // =========================
    // 第一阶段：初始化 GLFW + 创建窗口 + 加载 OpenGL
    // =========================
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // 开启深度测试：绘制 3D 立方体时，近处表面应遮挡远处表面
    glEnable(GL_DEPTH_TEST);

    // =========================
    // 第二阶段：编译链接着色器
    // =========================
    Shader ourShader("shaders/3.3.shader.vs", "shaders/3.3.shader.fs");

    // =========================
    // 第三阶段：准备立方体顶点数据 + 纹理，上传到 GPU
    // =========================

    // 立方体 6 个面 × 2 个三角形 × 3 个顶点 = 36 个顶点
    // 每个顶点 8 个 float = 位置(3) + 颜色(3) + UV(2)
    // 立方体中心在原点，边长 1.0（范围 [-0.5, 0.5]），这是**局部空间**坐标
    float vertices[] = {
        // ---- 位置 ----          ---- 颜色 ----        -- 纹理坐标 --
        // 后面 (z = -0.5)
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
        // 前面 (z = +0.5)
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        // 左面 (x = -0.5)
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
        // 右面 (x = +0.5)
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   1.0f, 1.0f, 0.0f,   1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        // 底面 (y = -0.5)
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 1.0f,   0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 1.0f,   1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 1.0f,   1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 1.0f,   1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,   1.0f, 0.0f, 1.0f,   0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   1.0f, 0.0f, 1.0f,   0.0f, 1.0f,
        // 顶面 (y = +0.5)
        -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 1.0f,   0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 1.0f,   1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 1.0f,   1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 1.0f,   1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,   0.0f, 1.0f, 1.0f,   0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f, 1.0f, 1.0f,   0.0f, 1.0f
    };

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 位置 attribute 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // 颜色 attribute 1
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // 纹理坐标 attribute 2
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // --- 创建并加载纹理 ---
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    int width, height, nrChannels;
    unsigned char* data = stbi_load("Resource/Texture/container.jpg", &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture: " << stbi_failure_reason() << std::endl;
    }
    stbi_image_free(data);

    // =========================
    // 坐标系矩阵（View / Projection 在循环外定义一次即可）
    // =========================

    // View 矩阵：定义「相机在哪、看向哪、上方向是哪」
    // 相机在 z=3 处看向原点，这样立方体（中心在原点）完整出现在视野中
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 3.0f),   // 相机位置（世界空间）
        glm::vec3(0.0f, 0.0f, 0.0f),   // 观察目标点
        glm::vec3(0.0f, 1.0f, 0.0f)    // 世界上方向
    );

    // Projection 矩阵：透视投影，把 3D 场景压成 2D 画面（近大远小）
    // aspect = 宽/高，窗口 resize 时需更新
    float aspect = (float)g_ScreenWidth / (float)g_ScreenHeight;
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),   // 垂直视野角度 FOV
        aspect,                // 宽高比
        0.1f,                  // 近裁剪面
        100.0f                 // 远裁剪面
    );

    // =========================
    // 第四阶段：渲染循环
    // =========================
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // --- Model 矩阵：每帧更新，实现随时间旋转 ---
        // 必须从单位矩阵开始，否则 glm::rotate 会在未初始化的矩阵上运算
        glm::mat4 model = glm::mat4(1.0f);
        // 先绕 X 轴倾斜 -55°，让立方体以更好角度展示（类似 LearnOpenGL 教程）
        model = glm::rotate(model, glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        // 再随时间绕 (0.5, 1.0, 0.0) 斜轴旋转，形成动态效果
        model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));

        // 窗口 resize 后，重新计算 aspect 和 projection
        aspect = (float)g_ScreenWidth / (float)g_ScreenHeight;
        projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        // 把 MVP 三个矩阵传给顶点着色器
        ourShader.setMat4("model", model);
        ourShader.setMat4("view", view);
        ourShader.setMat4("projection", projection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        ourShader.setInt("ourTexture", 0);

        glBindVertexArray(VAO);
        // 36 个顶点 = 12 个三角形 = 立方体 6 个面
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // =========================
    // 第五阶段：释放 GPU 资源
    // =========================
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &texture);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    g_ScreenWidth = static_cast<unsigned int>(width);
    g_ScreenHeight = static_cast<unsigned int>(height);
    glViewport(0, 0, width, height);
}
