// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

// ---------------------------------------------------------------------------
// Part4 Day02：最简模板测试 Demo（对照官网「矩形掩码」思路）
//
// 目标只有一句话：
//   模板测试「通过」→ 显示画面 A（橙色）
//   模板测试「不通过」→ 显示画面 B（蓝色）
//
// 做法分三步：
//   1) 把屏幕中央一块矩形的模板值写成 1（只打标签，不写颜色）
//   2) 用 GL_NOTEQUAL,1 画全屏 → 只有「不是 1」的像素留下 → 画面 B
//   3) 用 GL_EQUAL,1    画全屏 → 只有「等于 1」的像素留下 → 画面 A
// ---------------------------------------------------------------------------

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // GLFW 默认会创建模板缓冲；模板测试依赖它。

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Stencil Simple Demo", NULL, NULL);
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

    // 本 Demo 是 2D NDC 四边形，不需要深度测试；关掉以免干扰理解。
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_STENCIL_TEST);

    Shader colorShader("shaders/stencilColor.vs", "shaders/stencilColor.fs");

    // 全屏四边形（NDC：-1~1）——用来画「画面 A / 画面 B」
    float fullscreenQuad[] = {
        -1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f, -1.0f,

        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f
    };

    // 屏幕中央矩形 —— 只用来「盖章」：把这块区域的模板值写成 1
    // 改这四个角的坐标，就能改变通过区域的形状/大小。
    float maskQuad[] = {
        -0.45f,  0.55f,
        -0.45f, -0.55f,
         0.45f, -0.55f,

        -0.45f,  0.55f,
         0.45f, -0.55f,
         0.45f,  0.55f
    };

    unsigned int fullscreenVAO = 0, fullscreenVBO = 0;
    glGenVertexArrays(1, &fullscreenVAO);
    glGenBuffers(1, &fullscreenVBO);
    glBindVertexArray(fullscreenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenQuad), fullscreenQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    unsigned int maskVAO = 0, maskVBO = 0;
    glGenVertexArrays(1, &maskVAO);
    glGenBuffers(1, &maskVBO);
    glBindVertexArray(maskVAO);
    glBindBuffer(GL_ARRAY_BUFFER, maskVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(maskQuad), maskQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    std::cout << "Stencil Simple Demo\n"
              << "  center rect stencil = 1\n"
              << "  EQUAL 1  -> picture A (orange)\n"
              << "  NOTEQUAL 1 -> picture B (blue)\n"
              << "  ESC quit\n";

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // 颜色清成深灰；模板清成 0（整屏标签归零）
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        colorShader.use();

        // ============================================================
        // 第 1 步：写入模板掩码（中央矩形 → 模板值 1）
        // ============================================================
        // ALWAYS：矩形里的片段一律通过模板测试（我们就是要盖章）
        // REPLACE：测试通过时，把该像素模板值写成 ref=1
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilMask(0xFF); // 允许写入模板缓冲

        // 关掉颜色写入：这一遍只打标签，不在屏幕上留下痕迹
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        glBindVertexArray(maskVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 恢复颜色写入；后面两遍才是真正「画画」
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        // 后面只「读」模板，不再改标签（等价于深度里的 DepthMask(FALSE)）
        glStencilMask(0x00);

        // ============================================================
        // 第 2 步：模板「不通过」区域 → 画面 B（蓝色）
        // ============================================================
        // NOTEQUAL 1：只有模板值 != 1 的像素通过测试并着色
        // 中央矩形是 1，所以这里画的是「窗外」——蓝色画面 B
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        colorShader.setVec3("uColor", 0.15f, 0.35f, 0.85f); // 画面 B：蓝
        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ============================================================
        // 第 3 步：模板「通过」区域 → 画面 A（橙色）
        // ============================================================
        // EQUAL 1：只有模板值 == 1 的像素通过 → 中央橙色画面 A
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        colorShader.setVec3("uColor", 0.95f, 0.45f, 0.10f); // 画面 A：橙
        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 恢复掩码，避免影响下一帧或其它代码
        glStencilMask(0xFF);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &fullscreenVAO);
    glDeleteVertexArrays(1, &maskVAO);
    glDeleteBuffers(1, &fullscreenVBO);
    glDeleteBuffers(1, &maskVBO);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
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
