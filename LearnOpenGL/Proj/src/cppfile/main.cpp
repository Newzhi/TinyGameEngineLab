// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
// GLFW：创建窗口、处理输入、管理 OpenGL 上下文
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"

#include <iostream>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 场景颜色：珊瑚色物体 + 偏暖白光，便于观察明暗
glm::vec3 objectColor(1.0f, 0.5f, 0.31f);
glm::vec3 lightColor(1.0f, 1.0f, 1.0f);

// 光源初始位置（渲染循环中会绕物体旋转，方便演示漫反射）
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Phong Lighting", NULL, NULL);
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

    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
    g_camera = &camera;

    // 物体：Ambient + Diffuse + Specular；灯：显示 lightColor
    Shader lightingShader("shaders/lightTest.vs", "shaders/lightTest.fs");
    Shader lampShader("shaders/lamp.vs", "shaders/lamp.fs");

    // 每个顶点 6 float = 位置(3) + 法线(3)；法线按面给出，同一角点在不同面法线不同
    float vertices[] = {
        // 后面  z = -0.5，法线 (0, 0, -1)
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        // 前面  z = +0.5，法线 (0, 0, 1)
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        // 左面  x = -0.5，法线 (-1, 0, 0)
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

        // 右面  x = +0.5，法线 (1, 0, 0)
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        // 底面  y = -0.5，法线 (0, -1, 0)
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        // 顶面  y = +0.5，法线 (0, 1, 0)
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    unsigned int VBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);
    // location 0：位置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // location 1：法线
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 灯 VAO：同一 VBO，只读位置（stride 仍是 6 float，跳过法线）
    unsigned int lightVAO;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // 灯绕物体旋转，方便肉眼看到各面明暗随光方向变化
        lightPos.x = 1.2f * std::cos(currentFrame);
        lightPos.z = 1.2f * std::sin(currentFrame);
        lightPos.y = 1.0f;

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = (float)g_ScreenWidth / (float)g_ScreenHeight;
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);

        // ---------- 被照物体：Ambient + Diffuse + Specular ----------
        lightingShader.use();
        lightingShader.setVec3("objectColor", objectColor.x, objectColor.y, objectColor.z);
        lightingShader.setVec3("lightColor", lightColor.x, lightColor.y, lightColor.z);
        lightingShader.setVec3("lightPos", lightPos.x, lightPos.y, lightPos.z);
        // 高光需要视线方向：传入相机世界坐标
        lightingShader.setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("projection", projection);

        glm::mat4 model = glm::mat4(1.0f);
        lightingShader.setMat4("model", model);

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ---------- 灯立方体：标出当前光源位置与颜色 ----------
        lampShader.use();
        lampShader.setVec3("lightColor", lightColor.x, lightColor.y, lightColor.z);
        lampShader.setMat4("view", view);
        lampShader.setMat4("projection", projection);

        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lampShader.setMat4("model", model);

        glBindVertexArray(lightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

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
