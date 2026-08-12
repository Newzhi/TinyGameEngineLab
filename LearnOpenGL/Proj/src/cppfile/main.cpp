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
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char* path);
void setPointLight(const Shader& shader, int index, const glm::vec3& position,
                   const glm::vec3& color, float ambientScale, float diffuseScale);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const int NR_POINT_LIGHTS = 4;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool g_flashlightOn = true;
bool g_keyFWasDown = false;

// 教程常用四个点光位置
glm::vec3 pointLightPositions[NR_POINT_LIGHTS] = {
    glm::vec3( 0.7f,  0.2f,  2.0f),
    glm::vec3( 2.3f, -3.3f, -4.0f),
    glm::vec3(-4.0f,  2.0f, -12.0f),
    glm::vec3( 0.0f,  0.0f, -3.0f)
};

// 四盏点光用不同颜色，方便看出叠加
glm::vec3 pointLightColors[NR_POINT_LIGHTS] = {
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(1.0f, 0.3f, 0.1f),
    glm::vec3(0.2f, 0.5f, 1.0f),
    glm::vec3(0.3f, 1.0f, 0.4f)
};

void setPointLight(const Shader& shader, int index, const glm::vec3& position,
                   const glm::vec3& color, float ambientScale, float diffuseScale)
{
    std::string base = "pointLights[" + std::to_string(index) + "]";
    shader.setVec3(base + ".position", position.x, position.y, position.z);
    shader.setVec3(base + ".ambient",  color.x * ambientScale, color.y * ambientScale, color.z * ambientScale);
    shader.setVec3(base + ".diffuse",  color.x * diffuseScale, color.y * diffuseScale, color.z * diffuseScale);
    shader.setVec3(base + ".specular", color.x, color.y, color.z);
    shader.setFloat(base + ".constant",  1.0f);
    shader.setFloat(base + ".linear",    0.09f);
    shader.setFloat(base + ".quadratic", 0.032f);
}

void applyMultipleLights(const Shader& shader, const Camera& camera)
{
    // ---------- 平行光（太阳）----------
    shader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
    shader.setVec3("dirLight.ambient",  0.05f, 0.05f, 0.05f);
    shader.setVec3("dirLight.diffuse",  0.4f, 0.4f, 0.4f);
    shader.setVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);

    // ---------- 四个点光 ----------
    for (int i = 0; i < NR_POINT_LIGHTS; ++i)
        setPointLight(shader, i, pointLightPositions[i], pointLightColors[i], 0.05f, 0.8f);

    // ---------- 聚光（手电筒，跟相机）----------
    shader.setVec3("spotLight.position",  camera.Position.x, camera.Position.y, camera.Position.z);
    shader.setVec3("spotLight.direction", camera.Front.x, camera.Front.y, camera.Front.z);
    shader.setFloat("spotLight.cutOff",      std::cos(glm::radians(12.5f)));
    shader.setFloat("spotLight.outerCutOff", std::cos(glm::radians(17.5f)));
    shader.setFloat("spotLight.constant",  1.0f);
    shader.setFloat("spotLight.linear",    0.09f);
    shader.setFloat("spotLight.quadratic", 0.032f);

    if (g_flashlightOn)
    {
        shader.setVec3("spotLight.ambient",  0.0f, 0.0f, 0.0f);
        shader.setVec3("spotLight.diffuse",  1.0f, 1.0f, 1.0f);
        shader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
    }
    else
    {
        // 关闭：贡献为 0
        shader.setVec3("spotLight.ambient",  0.0f, 0.0f, 0.0f);
        shader.setVec3("spotLight.diffuse",  0.0f, 0.0f, 0.0f);
        shader.setVec3("spotLight.specular", 0.0f, 0.0f, 0.0f);
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Multiple Lights", NULL, NULL);
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
    lightingShader.setFloat("material.shininess", 32.0f);

    std::cout << "Multiple Lights Demo\n"
              << "  simultaneous: directional + 4 point lights + flashlight\n"
              << "  F  toggle flashlight\n"
              << "  WASD + mouse to move\n";

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = (float)g_ScreenWidth / (float)g_ScreenHeight;
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), aspect, 0.1f, 100.0f);

        lightingShader.use();
        applyMultipleLights(lightingShader, camera);
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

        // 四个点光小立方体（颜色与点光一致）
        lampShader.use();
        lampShader.setMat4("view", view);
        lampShader.setMat4("projection", projection);
        glBindVertexArray(lightVAO);
        for (int i = 0; i < NR_POINT_LIGHTS; ++i)
        {
            lampShader.setVec3("lightColor",
                               pointLightColors[i].x,
                               pointLightColors[i].y,
                               pointLightColors[i].z);
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pointLightPositions[i]);
            model = glm::scale(model, glm::vec3(0.2f));
            lampShader.setMat4("model", model);
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

    bool fDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fDown && !g_keyFWasDown)
    {
        g_flashlightOn = !g_flashlightOn;
        std::cout << "Flashlight -> " << (g_flashlightOn ? "ON" : "OFF") << std::endl;
    }
    g_keyFWasDown = fDown;
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
