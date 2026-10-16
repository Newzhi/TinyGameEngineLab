// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);
void renderScene(const Shader& shader, bool depthPass);
void renderCube();
void renderPlane();
void renderQuad();

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const unsigned int SHADOW_WIDTH = 1024;
const unsigned int SHADOW_HEIGHT = 1024;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 100.0f;
const float LIGHT_NEAR = 1.0f;
const float LIGHT_FAR = 7.5f;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 1 无阴影 / 2 硬阴影 / 3 PCF / 4 深度贴图可视化
int g_mode = 2;
bool g_key1WasDown = false;
bool g_key2WasDown = false;
bool g_key3WasDown = false;
bool g_key4WasDown = false;

const glm::vec3 LIGHT_POS(-2.0f, 4.0f, -1.0f);

unsigned int g_planeVAO = 0, g_planeVBO = 0;
unsigned int g_cubeVAO = 0, g_cubeVBO = 0;
unsigned int g_quadVAO = 0, g_quadVBO = 0;

// ---------------------------------------------------------------------------
// Part5 Day03：阴影映射
//
// Pass 1：把相机放到灯上，正交投影，只把最近深度写入 1024² 纹理。
// Pass 2：正常画场景，片元变到光源空间，和深度图比较：更远的就是影。
// 偏移消除摩尔纹；箱子深度 Pass 正面剔除减轻悬浮；边界钳制 + z>1 处理视锥外。
//
// 1 无阴影    2 硬阴影（默认）    3 PCF    4 全屏深度图
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
        SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Shadow Mapping", NULL, NULL);
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

    Shader depthShader("shaders/Part5-HighLevelLighting/Day03-ShadowMapping/shadowDepth.vs",
                       "shaders/Part5-HighLevelLighting/Day03-ShadowMapping/shadowDepth.fs");
    Shader sceneShader("shaders/Part5-HighLevelLighting/Day03-ShadowMapping/shadow.vs",
                       "shaders/Part5-HighLevelLighting/Day03-ShadowMapping/shadow.fs");
    Shader debugShader("shaders/Part5-HighLevelLighting/Day03-ShadowMapping/debugDepth.vs",
                       "shaders/Part5-HighLevelLighting/Day03-ShadowMapping/debugDepth.fs");
    Shader lampShader("shaders/common/lamp.vs", "shaders/common/lamp.fs");
    sceneShader.use();
    sceneShader.setInt("diffuseTexture", 0);
    sceneShader.setInt("shadowMap", 1);
    debugShader.use();
    debugShader.setInt("depthMap", 0);

    Camera camera(glm::vec3(0.0f, 2.0f, 6.0f));
    g_camera = &camera;

    unsigned int woodTexture = loadTexture("Resource/Texture/container.jpg");

    // ----- 只挂深度附件的 FBO：颜色读写关掉，否则不完整 -----
    unsigned int depthMapFBO = 0, depthMap = 0;
    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // 视锥外 UV 采到 1.0，不会被误判成阴影（GL_REPEAT 会在远处铺出假影子）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Shadow depth framebuffer is not complete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "Shadow Mapping Demo\n"
              << "  1 = lighting only (no shadows)\n"
              << "  2 = hard shadows + slope bias (default)\n"
              << "  3 = 3x3 PCF\n"
              << "  4 = visualize depth map (near=black)\n"
              << "  WASD + mouse, ESC quit\n";

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, LIGHT_NEAR, LIGHT_FAR);
        glm::mat4 lightView = glm::lookAt(LIGHT_POS, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // Pass 1：从灯看场景，只写最近深度
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        depthShader.use();
        depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        renderScene(depthShader, true);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glViewport(0, 0, static_cast<int>(g_ScreenWidth), static_cast<int>(g_ScreenHeight));
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (g_mode == 4)
        {
            debugShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            renderQuad();
        }
        else
        {
            glm::mat4 view = camera.GetViewMatrix();
            float aspect = static_cast<float>(g_ScreenWidth) / static_cast<float>(g_ScreenHeight);
            glm::mat4 projection = glm::perspective(
                glm::radians(camera.Zoom), aspect, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

            sceneShader.use();
            sceneShader.setMat4("view", view);
            sceneShader.setMat4("projection", projection);
            sceneShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
            sceneShader.setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
            sceneShader.setVec3("lightPos", LIGHT_POS.x, LIGHT_POS.y, LIGHT_POS.z);
            sceneShader.setInt("uShadowMode", g_mode == 1 ? 0 : (g_mode == 3 ? 2 : 1));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, woodTexture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depthMap);
            renderScene(sceneShader, false);

            lampShader.use();
            lampShader.setMat4("view", view);
            lampShader.setMat4("projection", projection);
            lampShader.setVec3("lightColor", 1.0f, 1.0f, 1.0f);
            glm::mat4 lampModel(1.0f);
            lampModel = glm::translate(lampModel, LIGHT_POS);
            lampModel = glm::scale(lampModel, glm::vec3(0.15f));
            lampShader.setMat4("model", lampModel);
            renderCube();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteVertexArrays(1, &g_planeVAO);
    glDeleteVertexArrays(1, &g_cubeVAO);
    glDeleteVertexArrays(1, &g_quadVAO);
    glDeleteBuffers(1, &g_planeVBO);
    glDeleteBuffers(1, &g_cubeVBO);
    glDeleteBuffers(1, &g_quadVBO);
    glDeleteTextures(1, &woodTexture);
    glDeleteTextures(1, &depthMap);
    glDeleteFramebuffers(1, &depthMapFBO);
    glfwTerminate();
    return 0;
}

void renderScene(const Shader& shader, bool depthPass)
{
    glm::mat4 model(1.0f);
    shader.setMat4("model", model);
    renderPlane();

    // 深度 Pass 对封闭箱子剔正面，减轻偏移造成的影子悬浮；地板是单面，不能剔。
    if (depthPass)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
    }

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 1.5f, 0.0f));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0f, 0.0f, 2.0f));
    model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f)));
    model = glm::scale(model, glm::vec3(0.25f));
    shader.setMat4("model", model);
    renderCube();

    if (depthPass)
    {
        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);
    }
}

void renderPlane()
{
    if (g_planeVAO == 0)
    {
        float planeVertices[] = {
             25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
            -25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
            -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

             25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
            -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
             25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
        };
        glGenVertexArrays(1, &g_planeVAO);
        glGenBuffers(1, &g_planeVBO);
        glBindVertexArray(g_planeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_planeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    }
    glBindVertexArray(g_planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void renderCube()
{
    if (g_cubeVAO == 0)
    {
        float vertices[] = {
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,

            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,

            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,

             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,

            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,

            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
        };
        glGenVertexArrays(1, &g_cubeVAO);
        glGenBuffers(1, &g_cubeVBO);
        glBindVertexArray(g_cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    }
    glBindVertexArray(g_cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void renderQuad()
{
    if (g_quadVAO == 0)
    {
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &g_quadVAO);
        glGenBuffers(1, &g_quadVBO);
        glBindVertexArray(g_quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    glBindVertexArray(g_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

unsigned int loadTexture(const char* path)
{
    unsigned int textureID = 0;
    glGenTextures(1, &textureID);

    int width = 0, height = 0, nrComponents = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<int>(format), width, height, 0,
                     format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
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
        std::cout << "[shadow] off\n";
    }
    if (edge(window, GLFW_KEY_2, g_key2WasDown))
    {
        g_mode = 2;
        std::cout << "[shadow] hard + bias\n";
    }
    if (edge(window, GLFW_KEY_3, g_key3WasDown))
    {
        g_mode = 3;
        std::cout << "[shadow] 3x3 PCF\n";
    }
    if (edge(window, GLFW_KEY_4, g_key4WasDown))
    {
        g_mode = 4;
        std::cout << "[shadow] depth map debug\n";
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
