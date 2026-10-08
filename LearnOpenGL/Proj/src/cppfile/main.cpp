// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"
#include "../headfile/Model.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void setupRockInstanceAttribs(Model& rock, unsigned int instanceVBO);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 400.0f;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 1 实例化一次提交 / 2 循环多次 Draw（对比 draw call）
int g_mode = 1;
bool g_key1WasDown = false;
bool g_key2WasDown = false;

const unsigned int ASTEROID_COUNT = 10000;
const unsigned int NAIVE_COUNT = 500;

// ---------------------------------------------------------------------------
// Part4 Day10：实例化 Instancing —— 小行星带
//
// 同一份 rock.obj，10000 个不同的 model 矩阵放进实例 VBO。
// glVertexAttribDivisor(loc, 1)：这个属性每个实例更新一次，不是每个顶点一次。
// glDrawElementsInstanced(..., ASTEROID_COUNT)：一次 draw call 画出整圈。
// 按 2 改成循环 500 次普通 Draw，能感到 draw call 变多之后的差别。
// ---------------------------------------------------------------------------

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
        SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Instancing / Asteroid Belt", NULL, NULL);
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

    Shader planetShader("shaders/planet.vs", "shaders/planet.fs");
    Shader asteroidShader("shaders/asteroid.vs", "shaders/asteroid.fs");

    Model planet("Resource/TestLoadModel/planet.obj");
    Model rock("Resource/TestLoadModel/rock.obj");

    Camera camera(glm::vec3(0.0f, 8.0f, 55.0f));
    g_camera = &camera;

    // 在半径 50 的圆环上撒点，再加位移 / 缩放 / 旋转，让每颗石头不一样
    std::vector<glm::mat4> modelMatrices(ASTEROID_COUNT);
    srand(static_cast<unsigned int>(glfwGetTime()));
    const float radius = 50.0f;
    const float offset = 12.5f;
    for (unsigned int i = 0; i < ASTEROID_COUNT; ++i)
    {
        glm::mat4 model(1.0f);
        const float angle = static_cast<float>(i) / static_cast<float>(ASTEROID_COUNT) * 2.0f * 3.14159265f;
        float displacement = (rand() % static_cast<int>(2 * offset * 100)) / 100.0f - offset;
        const float x = std::sin(angle) * radius + displacement;
        displacement = (rand() % static_cast<int>(2 * offset * 100)) / 100.0f - offset;
        const float y = displacement * 0.4f;
        displacement = (rand() % static_cast<int>(2 * offset * 100)) / 100.0f - offset;
        const float z = std::cos(angle) * radius + displacement;
        model = glm::translate(model, glm::vec3(x, y, z));

        const float scale = (rand() % 20) / 100.0f + 0.05f;
        model = glm::scale(model, glm::vec3(scale));

        const float rotAngle = static_cast<float>(rand() % 360);
        model = glm::rotate(model, glm::radians(rotAngle), glm::vec3(0.4f, 0.6f, 0.8f));

        modelMatrices[i] = model;
    }

    unsigned int instanceVBO = 0;
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(ASTEROID_COUNT * sizeof(glm::mat4)),
                 modelMatrices.data(),
                 GL_STATIC_DRAW);
    setupRockInstanceAttribs(rock, instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    std::cout << "Instancing / Asteroid Belt Demo\n"
              << "  1 = glDrawElementsInstanced (" << ASTEROID_COUNT << " rocks, 1 draw call)\n"
              << "  2 = naive loop (" << NAIVE_COUNT << " rocks, " << NAIVE_COUNT << " draw calls)\n"
              << "  WASD + mouse, ESC quit\n";

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = static_cast<float>(g_ScreenWidth) / static_cast<float>(g_ScreenHeight);
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), aspect, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

        planetShader.use();
        planetShader.setMat4("view", view);
        planetShader.setMat4("projection", projection);
        glm::mat4 planetModel(1.0f);
        planetModel = glm::translate(planetModel, glm::vec3(0.0f, -3.0f, 0.0f));
        planetModel = glm::scale(planetModel, glm::vec3(4.0f));
        planetShader.setMat4("model", planetModel);
        planet.Draw(planetShader);

        if (g_mode == 1)
        {
            asteroidShader.use();
            asteroidShader.setMat4("view", view);
            asteroidShader.setMat4("projection", projection);
            rock.DrawInstanced(asteroidShader, ASTEROID_COUNT);
        }
        else
        {
            // 非实例化绘制读的是 uniform model。若仍用 asteroid.vs，
            // gl_InstanceID 恒为 0，所有石头都会用实例缓冲里的第一份矩阵。
            planetShader.use();
            planetShader.setMat4("view", view);
            planetShader.setMat4("projection", projection);
            for (unsigned int i = 0; i < NAIVE_COUNT; ++i)
            {
                planetShader.setMat4("model", modelMatrices[i]);
                rock.Draw(planetShader);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteBuffers(1, &instanceVBO);
    glfwTerminate();
    return 0;
}

// mat4 拆成 4 个 vec4 属性，divisor=1：每个实例换一次矩阵，顶点之间共用
void setupRockInstanceAttribs(Model& rock, unsigned int instanceVBO)
{
    const std::size_t vec4Size = sizeof(glm::vec4);
    for (unsigned int i = 0; i < rock.meshes.size(); ++i)
    {
        glBindVertexArray(rock.meshes[i].GetVAO());
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              (void*)(1 * vec4Size));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              (void*)(2 * vec4Size));
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              (void*)(3 * vec4Size));

        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);
        glVertexAttribDivisor(6, 1);
    }
    glBindVertexArray(0);
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
        std::cout << "[Instancing] DrawInstanced " << ASTEROID_COUNT << "\n";
    }
    if (edge(window, GLFW_KEY_2, g_key2WasDown))
    {
        g_mode = 2;
        std::cout << "[Instancing] naive loop " << NAIVE_COUNT << "\n";
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
