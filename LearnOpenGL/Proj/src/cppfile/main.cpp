// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void recreateFramebuffers(int width, int height);
void drawCube(const Shader& shader, unsigned int vao, const glm::mat4& view, const glm::mat4& projection);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 100.0f;
const int MSAA_SAMPLES = 4;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 1 无 MSAA（锯齿） / 2 离屏 4x MSAA 再 blit 到窗口 / 3 MSAA 还原成 2D 纹理后全屏采样（可灰度）
int g_mode = 2;
bool g_key1WasDown = false;
bool g_key2WasDown = false;
bool g_key3WasDown = false;

unsigned int g_msaaFBO = 0, g_msaaColor = 0, g_msaaRBO = 0;
unsigned int g_resolveFBO = 0, g_resolveColor = 0;

// ---------------------------------------------------------------------------
// Part4 Day11：抗锯齿 Anti-Aliasing —— 离屏 MSAA
//
// 窗口本身不请求多重采样缓冲（GLFW_SAMPLES 保持默认 0），对比才看得见。
//   1) 直接画到默认帧缓冲：每个像素 1 个采样点，边缘锯齿
//   2) 画到 4x 多重采样 FBO，glBlitFramebuffer 还原到窗口：边缘被覆盖率混合平滑
//   3) 先 blit 到普通 2D 纹理 FBO，再全屏四边形采样（演示「不能直接采样 MSAA 纹理」）
//
// MSAA：每个像素多个子采样点决定覆盖率；每个图元每个像素仍只跑一次片元着色器。
// ---------------------------------------------------------------------------

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // 不设 GLFW_SAMPLES：默认颜色缓冲每像素 1 个样本，模式 1 才能看出锯齿。
    // 若在这里写 glfwWindowHint(GLFW_SAMPLES, 4)，默认帧缓冲本身就是 4x MSAA。

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Anti-Aliasing / MSAA", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    // 画到多重采样附件时，显式打开。多数驱动默认已开，写上更保险。
    glEnable(GL_MULTISAMPLE);

    Shader cubeShader("shaders/msaa.vs", "shaders/msaa.fs");
    Shader screenShader("shaders/msaaScreen.vs", "shaders/msaaScreen.fs");
    screenShader.use();
    screenShader.setInt("screenTexture", 0);

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
    g_camera = &camera;

    // 位置 + 颜色。六个面颜色不同，斜看时轮廓锯齿更明显
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.20f, 0.75f, 0.30f,
         0.5f, -0.5f, -0.5f,  0.20f, 0.75f, 0.30f,
         0.5f,  0.5f, -0.5f,  0.20f, 0.75f, 0.30f,
         0.5f,  0.5f, -0.5f,  0.20f, 0.75f, 0.30f,
        -0.5f,  0.5f, -0.5f,  0.20f, 0.75f, 0.30f,
        -0.5f, -0.5f, -0.5f,  0.20f, 0.75f, 0.30f,

        -0.5f, -0.5f,  0.5f,  0.85f, 0.35f, 0.20f,
         0.5f, -0.5f,  0.5f,  0.85f, 0.35f, 0.20f,
         0.5f,  0.5f,  0.5f,  0.85f, 0.35f, 0.20f,
         0.5f,  0.5f,  0.5f,  0.85f, 0.35f, 0.20f,
        -0.5f,  0.5f,  0.5f,  0.85f, 0.35f, 0.20f,
        -0.5f, -0.5f,  0.5f,  0.85f, 0.35f, 0.20f,

        -0.5f,  0.5f,  0.5f,  0.25f, 0.45f, 0.90f,
        -0.5f,  0.5f, -0.5f,  0.25f, 0.45f, 0.90f,
        -0.5f, -0.5f, -0.5f,  0.25f, 0.45f, 0.90f,
        -0.5f, -0.5f, -0.5f,  0.25f, 0.45f, 0.90f,
        -0.5f, -0.5f,  0.5f,  0.25f, 0.45f, 0.90f,
        -0.5f,  0.5f,  0.5f,  0.25f, 0.45f, 0.90f,

         0.5f,  0.5f,  0.5f,  0.90f, 0.80f, 0.20f,
         0.5f,  0.5f, -0.5f,  0.90f, 0.80f, 0.20f,
         0.5f, -0.5f, -0.5f,  0.90f, 0.80f, 0.20f,
         0.5f, -0.5f, -0.5f,  0.90f, 0.80f, 0.20f,
         0.5f, -0.5f,  0.5f,  0.90f, 0.80f, 0.20f,
         0.5f,  0.5f,  0.5f,  0.90f, 0.80f, 0.20f,

        -0.5f, -0.5f, -0.5f,  0.70f, 0.25f, 0.70f,
         0.5f, -0.5f, -0.5f,  0.70f, 0.25f, 0.70f,
         0.5f, -0.5f,  0.5f,  0.70f, 0.25f, 0.70f,
         0.5f, -0.5f,  0.5f,  0.70f, 0.25f, 0.70f,
        -0.5f, -0.5f,  0.5f,  0.70f, 0.25f, 0.70f,
        -0.5f, -0.5f, -0.5f,  0.70f, 0.25f, 0.70f,

        -0.5f,  0.5f, -0.5f,  0.20f, 0.85f, 0.80f,
         0.5f,  0.5f, -0.5f,  0.20f, 0.85f, 0.80f,
         0.5f,  0.5f,  0.5f,  0.20f, 0.85f, 0.80f,
         0.5f,  0.5f,  0.5f,  0.20f, 0.85f, 0.80f,
        -0.5f,  0.5f,  0.5f,  0.20f, 0.85f, 0.80f,
        -0.5f,  0.5f, -0.5f,  0.20f, 0.85f, 0.80f
    };

    unsigned int cubeVAO = 0, cubeVBO = 0;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO = 0, quadVBO = 0;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    recreateFramebuffers(static_cast<int>(g_ScreenWidth), static_cast<int>(g_ScreenHeight));

    std::cout << "Anti-Aliasing / MSAA Demo\n"
              << "  1 = no MSAA (jagged edges on default framebuffer)\n"
              << "  2 = 4x MSAA FBO, blit resolve to window (default)\n"
              << "  3 = MSAA resolve to 2D texture, then fullscreen sample (grayscale)\n"
              << "  look at cube silhouette; WASD + mouse, ESC quit\n";

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = static_cast<float>(g_ScreenWidth) / static_cast<float>(g_ScreenHeight);
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), aspect, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

        const int w = static_cast<int>(g_ScreenWidth);
        const int h = static_cast<int>(g_ScreenHeight);

        if (g_mode == 1)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, w, h);
            glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            drawCube(cubeShader, cubeVAO, view, projection);
        }
        else
        {
            // 画进 4x 多重采样 FBO：光栅器按覆盖率写入各个子样本
            glBindFramebuffer(GL_FRAMEBUFFER, g_msaaFBO);
            glViewport(0, 0, w, h);
            glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            drawCube(cubeShader, cubeVAO, view, projection);

            if (g_mode == 2)
            {
                // 还原(resolve)：把多重采样颜色平均成单样本，拷到默认帧缓冲
                glBindFramebuffer(GL_READ_FRAMEBUFFER, g_msaaFBO);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }
            else
            {
                // 先还原到普通 2D 纹理，才能在片元着色器里 texture()
                glBindFramebuffer(GL_READ_FRAMEBUFFER, g_msaaFBO);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_resolveFBO);
                glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, w, h);
                glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                glDisable(GL_DEPTH_TEST);

                screenShader.use();
                screenShader.setInt("uGray", 1);
                glBindVertexArray(quadVAO);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, g_resolveColor);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glEnable(GL_DEPTH_TEST);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &quadVBO);
    recreateFramebuffers(0, 0);
    glfwTerminate();
    return 0;
}

void drawCube(const Shader& shader, unsigned int vao, const glm::mat4& view, const glm::mat4& projection)
{
    shader.use();
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    glm::mat4 model(1.0f);
    model = glm::rotate(model, glm::radians(35.0f), glm::vec3(0.4f, 1.0f, 0.2f));
    shader.setMat4("model", model);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void recreateFramebuffers(int width, int height)
{
    if (g_msaaFBO != 0)
    {
        glDeleteFramebuffers(1, &g_msaaFBO);
        glDeleteTextures(1, &g_msaaColor);
        glDeleteRenderbuffers(1, &g_msaaRBO);
        glDeleteFramebuffers(1, &g_resolveFBO);
        glDeleteTextures(1, &g_resolveColor);
        g_msaaFBO = g_msaaColor = g_msaaRBO = 0;
        g_resolveFBO = g_resolveColor = 0;
    }
    if (width <= 0 || height <= 0)
        return;

    // ----- 多重采样 FBO：颜色用多重采样纹理，深度/模板用多重采样 RBO -----
    glGenFramebuffers(1, &g_msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_msaaFBO);

    glGenTextures(1, &g_msaaColor);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, g_msaaColor);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_SAMPLES, GL_RGB, width, height, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D_MULTISAMPLE, g_msaaColor, 0);

    glGenRenderbuffers(1, &g_msaaRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, g_msaaRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, MSAA_SAMPLES, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_msaaRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "MSAA framebuffer is not complete\n";

    // ----- 普通 FBO：接收 blit 还原后的单样本颜色，给全屏四边形采样 -----
    glGenFramebuffers(1, &g_resolveFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g_resolveFBO);
    glGenTextures(1, &g_resolveColor);
    glBindTexture(GL_TEXTURE_2D, g_resolveColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_resolveColor, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Resolve framebuffer is not complete\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    auto edge = [](GLFWwindow* w, int key, bool& wasDown) -> bool {
        bool down = glfwGetKey(w, key) == GLFW_PRESS;
        bool fired = down && !wasDown;
        wasDown = down;
        return fired;
    };

    if (edge(window, GLFW_KEY_1, g_key1WasDown))
    {
        g_mode = 1;
        std::cout << "[MSAA] off (jagged)\n";
    }
    if (edge(window, GLFW_KEY_2, g_key2WasDown))
    {
        g_mode = 2;
        std::cout << "[MSAA] 4x FBO + blit to window\n";
    }
    if (edge(window, GLFW_KEY_3, g_key3WasDown))
    {
        g_mode = 3;
        std::cout << "[MSAA] resolve to texture + grayscale quad\n";
    }

    if (g_camera == nullptr)
        return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        g_camera->ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        g_camera->ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        g_camera->ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        g_camera->ProcessKeyboard(RIGHT, deltaTime);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    (void)window;
    if (g_camera != nullptr)
        g_camera->ProcessMouseMovement(xpos, ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window;
    (void)xoffset;
    if (g_camera != nullptr)
        g_camera->ProcessMouseScroll(static_cast<float>(yoffset));
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    (void)window;
    g_ScreenWidth = static_cast<unsigned int>(width);
    g_ScreenHeight = static_cast<unsigned int>(height);
    glViewport(0, 0, width, height);
    recreateFramebuffers(width, height);
}
