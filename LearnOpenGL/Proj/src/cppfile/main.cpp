// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
// GLFW：创建窗口、处理输入、管理 OpenGL 上下文
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"
#include "stb_image.h"

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char* path);
void printLightMode();

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
glm::vec3 lightPos(1.2f, 1.0f, 2.0f);
glm::vec3 lightDir(-0.2f, -1.0f, -0.3f); // 平行光：从天空朝下

// 0 平行光 | 1 点光 | 2 聚光（手电筒）
int g_lightType = 0;
bool g_key1WasDown = false;
bool g_key2WasDown = false;
bool g_key3WasDown = false;

const char* lightTypeName(int type)
{
    switch (type)
    {
    case 0:  return "Directional (parallel / sun)";
    case 1:  return "Point (with attenuation)";
    case 2:  return "Spotlight (flashlight)";
    default: return "Unknown";
    }
}

void printLightMode()
{
    std::cout << "Light -> " << lightTypeName(g_lightType) << std::endl;
}

void applyLightUniforms(const Shader& shader, const Camera& camera)
{
    shader.setInt("lightType", g_lightType);
    shader.setVec3("light.ambient",  lightColor.x * 0.1f, lightColor.y * 0.1f, lightColor.z * 0.1f);
    shader.setVec3("light.diffuse",  lightColor.x * 0.8f, lightColor.y * 0.8f, lightColor.z * 0.8f);
    shader.setVec3("light.specular", lightColor.x, lightColor.y, lightColor.z);

    // 点光 / 聚光衰减：约覆盖距离 50（Ogre 表）
    shader.setFloat("light.constant",  1.0f);
    shader.setFloat("light.linear",    0.09f);
    shader.setFloat("light.quadratic", 0.032f);

    // 聚光内外切光角（传余弦）
    shader.setFloat("light.cutOff",      std::cos(glm::radians(12.5f)));
    shader.setFloat("light.outerCutOff", std::cos(glm::radians(17.5f)));

    if (g_lightType == 0)
    {
        shader.setVec3("light.direction", lightDir.x, lightDir.y, lightDir.z);
    }
    else if (g_lightType == 1)
    {
        shader.setVec3("light.position", lightPos.x, lightPos.y, lightPos.z);
    }
    else // 2 手电筒：跟相机
    {
        shader.setVec3("light.position",  camera.Position.x, camera.Position.y, camera.Position.z);
        shader.setVec3("light.direction", camera.Front.x, camera.Front.y, camera.Front.z);
    }
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Light Casters", NULL, NULL);
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

    Camera camera(glm::vec3(0.0f, 0.0f, 6.0f));
    g_camera = &camera;

    Shader lightingShader("shaders/lightTest.vs", "shaders/lightTest.fs");
    Shader lampShader("shaders/lamp.vs", "shaders/lamp.fs");

    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
    };

    // 随机生成多个箱子位置 + 旋转轴（固定种子，便于复现）
    const int cubeCount = 10;
    std::vector<glm::vec3> cubePositions;
    std::vector<float> cubeAngles;
    std::vector<glm::vec3> cubeAxes;
    std::srand(static_cast<unsigned int>(42));
    for (int i = 0; i < cubeCount; ++i)
    {
        float x = (static_cast<float>(std::rand() % 1001) / 1000.0f) * 10.0f - 5.0f;
        float y = (static_cast<float>(std::rand() % 1001) / 1000.0f) * 6.0f  - 2.0f;
        float z = (static_cast<float>(std::rand() % 1001) / 1000.0f) * 10.0f - 8.0f;
        cubePositions.emplace_back(x, y, z);
        cubeAngles.push_back(static_cast<float>(std::rand() % 360));
        float ax = (static_cast<float>(std::rand() % 100) + 1) / 100.0f;
        float ay = (static_cast<float>(std::rand() % 100) + 1) / 100.0f;
        float az = (static_cast<float>(std::rand() % 100) + 1) / 100.0f;
        cubeAxes.emplace_back(glm::normalize(glm::vec3(ax, ay, az)));
    }

    unsigned int VBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int lightVAO;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int diffuseMap  = loadTexture("Resource/Texture/container2.png");
    unsigned int specularMap = loadTexture("Resource/Texture/container2_specular.png");

    lightingShader.use();
    lightingShader.setInt("material.diffuse", 0);
    lightingShader.setInt("material.specular", 1);
    lightingShader.setFloat("material.shininess", 64.0f);

    std::cout << "Light Casters Demo — " << cubeCount << " random cubes\n"
              << "  1 Directional light (sun)\n"
              << "  2 Point light + attenuation (lamp cube orbits)\n"
              << "  3 Spotlight / flashlight (follows camera)\n"
              << "  WASD + mouse to move\n";
    printLightMode();

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // 点光：灯绕场景中心旋转，便于看出近亮远暗
        lightPos.x = 2.0f * std::cos(currentFrame * 0.8f);
        lightPos.z = 2.0f * std::sin(currentFrame * 0.8f);
        lightPos.y = 1.5f;

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = (float)g_ScreenWidth / (float)g_ScreenHeight;
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);

        lightingShader.use();
        applyLightUniforms(lightingShader, camera);
        lightingShader.setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        lightingShader.setMat4("view", view);
        lightingShader.setMat4("projection", projection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);

        glBindVertexArray(cubeVAO);
        for (int i = 0; i < cubeCount; ++i)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            float angle = cubeAngles[i] + currentFrame * 15.0f;
            model = glm::rotate(model, glm::radians(angle), cubeAxes[i]);
            lightingShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // 仅点光模式画出灯立方体
        if (g_lightType == 1)
        {
            lampShader.use();
            lampShader.setVec3("lightColor", lightColor.x, lightColor.y, lightColor.z);
            lampShader.setMat4("view", view);
            lampShader.setMat4("projection", projection);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, lightPos);
            model = glm::scale(model, glm::vec3(0.2f));
            lampShader.setMat4("model", model);

            glBindVertexArray(lightVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &diffuseMap);
    glDeleteTextures(1, &specularMap);
    glfwTerminate();
    return 0;
}

unsigned int loadTexture(const char* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load texture: " << path
                  << " (" << stbi_failure_reason() << ")" << std::endl;
        stbi_image_free(data);
    }

    return textureID;
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

    auto trySwitch = [](GLFWwindow* win, int key, bool& wasDown, int type) {
        bool down = glfwGetKey(win, key) == GLFW_PRESS;
        if (down && !wasDown)
        {
            g_lightType = type;
            printLightMode();
        }
        wasDown = down;
    };

    trySwitch(window, GLFW_KEY_1, g_key1WasDown, 0);
    trySwitch(window, GLFW_KEY_2, g_key2WasDown, 1);
    trySwitch(window, GLFW_KEY_3, g_key3WasDown, 2);
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
