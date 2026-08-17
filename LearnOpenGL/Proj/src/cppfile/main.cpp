// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"

#include <stb_image.h>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void applyCullMode();
unsigned int loadTexture(const char* path);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 100.0f;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 演示用：当前剔除策略（由数字键切换）
enum class CullMode
{
    Off = 0,     // 1：关剔除 —— 正反面都画，片元都会进 FS
    CullBack,    // 2：剔背面（默认）—— 屏幕上呈顺时针的三角丢掉
    CullFront    // 3：剔正面 —— 外面看不见，飞进去能看到「内壁」
};

// 教学 Demo 默认剔除正面：启动后直接拿掉靠近相机的外壳，
// 因而能看到立方体后方保留下来的背面；按 2 可切回常规的背面剔除。
CullMode g_cullMode = CullMode::CullFront;

bool g_key1WasDown = false;
bool g_key2WasDown = false;
bool g_key3WasDown = false;

// ---------------------------------------------------------------------------
// Part4 Day04：面剔除 Face Culling
//
// 核心问题：
//   1) OpenGL 怎么知道一个三角形朝向相机还是背对相机？
//      → 不看法线，而看「投影到屏幕后三个顶点是逆时针还是顺时针」
//   2) 为什么有的面「片元不显示」？
//      → 被判定为要剔除的那一类面：整三角在光栅化后丢弃，FS 基本不跑，
//         也不写颜色/深度 —— 不是画了透明，是根本没留下来
//
// Demo：只画一个闭合立方体，每个面从外侧看都是 CCW。
//   CullBack ：丢掉背向相机的面，只看到立方体外壳（正常效果）
//   CullFront：丢掉朝向相机的面，前壳被拿走，于是能看到后面的内侧
//   Off      ：正反面都参与光栅化；深度测试仍会让近处外壳挡住后面
// ---------------------------------------------------------------------------

void applyCullMode()
{
    if (g_cullMode == CullMode::Off)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(g_cullMode == CullMode::CullBack ? GL_BACK : GL_FRONT);
    }

    // 官网默认约定：投影到屏幕后，逆时针（CCW）环绕的三角形是正面。
    // 本 Demo 的立方体数据严格按这个约定书写，不再切换 CW，避免混淆。
    glFrontFace(GL_CCW);

    const char* cullStr =
        (g_cullMode == CullMode::Off) ? "OFF" :
        (g_cullMode == CullMode::CullBack) ? "CULL BACK" : "CULL FRONT";
    std::cout << "[FaceCull] " << cullStr << " | front face = CCW" << std::endl;
}

// 本章只需要一个最小纹理加载器。
// 图片负责让面的方向更容易辨认；面剔除仍只由顶点环绕顺序决定，与纹理无关。
unsigned int loadTexture(const char* path)
{
    unsigned int texture = 0;
    glGenTextures(1, &texture);

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    if (data == nullptr)
    {
        std::cout << "Failed to load texture: " << path << std::endl;
        return texture;
    }

    GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format),
                 width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);

    return texture;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(
        SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Face Culling", NULL, NULL);
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
    applyCullMode();

    Camera camera(glm::vec3(0.0f, 0.0f, 4.0f));
    g_camera = &camera;

    Shader shader("shaders/faceCull.vs", "shaders/faceCull.fs");

    // ========================================================================
    // 单个 CCW 立方体（参考官网：从每个面的外侧看，顶点都是逆时针）
    // ------------------------------------------------------------------------
    // 每个面的 2 个三角形，都按「站在该面外侧朝里看」写成逆时针。
    //
    // 默认 glFrontFace(GL_CCW)：
    //   - 从相机看到的外侧面 → 屏幕上仍是 CCW → 判为 FRONT → 保留
    //   - 背对相机的那一侧 → 屏幕上变成 CW → 判为 BACK → 被 glCullFace(BACK) 丢掉
    //
    // 所以：外面能看见最多 3 个面；飞进内部后，原来的内壁相对你变成 BACK → 全没了
    // ========================================================================
    float cubeCCW[] = {
        // 位置                    // UV
        // 每个面从「立方体外侧」看都是 CCW；UV 只用于显示同一张纹理。

        // 前面 +Z
        -0.5f, -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 0.0f,

        // 后面 -Z：站在 -Z 外侧看仍为 CCW
         0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,     0.0f, 0.0f,

        // 左面 -X
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,     1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 0.0f,

        // 右面 +X
         0.5f, -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,     0.0f, 0.0f,

        // 下面 -Y
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,     0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,     0.0f, 0.0f,

        // 上面 +Y
        -0.5f,  0.5f,  0.5f,     0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,     1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,     0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,     1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,     0.0f, 0.0f
    };

    unsigned int cubeVAO = 0, cubeVBO = 0;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeCCW), cubeCCW, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    unsigned int cubeTexture = loadTexture("Resource/Texture/container2.png");
    shader.use();
    shader.setInt("texture1", 0);

    std::cout << "Face Culling Demo\n"
              << "  1=cull OFF  2=cull BACK  3=cull FRONT\n"
              << "  Cull BACK : see the normal outer cube\n"
              << "  Cull FRONT: front shell disappears; see back/inside faces\n"
              << "  WASD + mouse, ESC quit\n";

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = static_cast<float>(g_ScreenWidth) / static_cast<float>(g_ScreenHeight);
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), aspect, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

        shader.use();
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        // 只画一个立方体：
        // - 按 2：正面保留、背面在光栅化前被丢掉 → 正常实心外观
        // - 按 3：正面被丢掉 → 近处外壳不产生片元，也不写深度；
        //         后方原本朝外的面，从当前相机看属于背面，因此被保留并显示
        {
            glm::mat4 model(1.0f);
            // 稍微旋转，启动时同时看见前、右、上三个面，朝向变化更直观。
            model = glm::rotate(model, glm::radians(22.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            shader.setMat4("model", model);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cubeTexture);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteTextures(1, &cubeTexture);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // 数字键边沿触发，避免按住连刷日志
    auto edge = [](GLFWwindow* w, int key, bool& wasDown) -> bool {
        bool down = glfwGetKey(w, key) == GLFW_PRESS;
        bool fired = down && !wasDown;
        wasDown = down;
        return fired;
    };

    if (edge(window, GLFW_KEY_1, g_key1WasDown))
    {
        g_cullMode = CullMode::Off;
        applyCullMode();
    }
    if (edge(window, GLFW_KEY_2, g_key2WasDown))
    {
        g_cullMode = CullMode::CullBack;
        applyCullMode();
    }
    if (edge(window, GLFW_KEY_3, g_key3WasDown))
    {
        g_cullMode = CullMode::CullFront;
        applyCullMode();
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
}
