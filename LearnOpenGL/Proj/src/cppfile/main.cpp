// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"

#include <iostream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path, bool srgb);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 100.0f;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// false：旧管线（RGB 贴图 + 1/d） / true：线性工作流（sRGB 贴图 + 1/d² + 输出校正）
bool g_gamma = false;
bool g_key1WasDown = false;
bool g_key2WasDown = false;
bool g_keySpaceWasDown = false;

const glm::vec3 LIGHT_POSITIONS[4] = {
    glm::vec3(-3.0f, 0.0f, 0.0f),
    glm::vec3(-1.0f, 0.0f, 0.0f),
    glm::vec3( 1.0f, 0.0f, 0.0f),
    glm::vec3( 3.0f, 0.0f, 0.0f)
};
const glm::vec3 LIGHT_COLORS[4] = {
    glm::vec3(0.25f),
    glm::vec3(0.50f),
    glm::vec3(0.75f),
    glm::vec3(1.00f)
};

// ---------------------------------------------------------------------------
// Part5 Day02：Gamma 校正
//
// 显示器大约按 2.2 次幂压暗中间亮度。不校正时，中间调偏暗，1/d² 衰减会显得过狠。
// 打开后：漫反射用 GL_SRGB 解回线性 → 着色器按物理算（含 1/d²）→ 输出 pow(1/2.2)。
// 只在上屏前校正一次；albedo 才能标 sRGB，法线 / 高光贴图不行。
//
// 1 关校正（默认）    2 或空格：开校正
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
        SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Gamma Correction", NULL, NULL);
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

    Shader floorShader("shaders/gamma.vs", "shaders/gamma.fs");
    Shader lampShader("shaders/lamp.vs", "shaders/lamp.fs");
    floorShader.use();
    floorShader.setInt("floorTexture", 0);

    Camera camera(glm::vec3(0.0f, 0.8f, 6.0f));
    g_camera = &camera;

    float planeVertices[] = {
         10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f,  0.0f,
        -10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
        -10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,   0.0f, 10.0f,

         10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f,  0.0f,
        -10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,   0.0f, 10.0f,
         10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,  10.0f, 10.0f
    };

    unsigned int planeVAO = 0, planeVBO = 0;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
    };
    unsigned int cubeVAO = 0, cubeVBO = 0;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // 同一张图两份内部格式：关校正当线性字节用；开校正由驱动解回线性，避免 Gamma 两次。
    unsigned int floorLinear = loadTexture("Resource/Texture/container.jpg", false);
    unsigned int floorSRGB = loadTexture("Resource/Texture/container.jpg", true);

    std::cout << "Gamma Correction Demo\n"
              << "  1     = off: GL_RGB texture, 1/d attenuation, no output pow\n"
              << "  2 / Space = on: GL_SRGB texture, 1/d^2, pow(1/2.2) before display\n"
              << "  step back and compare how far each lamp reaches\n"
              << "  WASD + mouse, ESC quit\n";

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = static_cast<float>(g_ScreenWidth) / static_cast<float>(g_ScreenHeight);
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), aspect, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

        floorShader.use();
        floorShader.setMat4("view", view);
        floorShader.setMat4("projection", projection);
        glm::mat4 model(1.0f);
        floorShader.setMat4("model", model);
        floorShader.setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        floorShader.setBool("gamma", g_gamma);
        for (int i = 0; i < 4; ++i)
        {
            std::string idx = std::to_string(i);
            floorShader.setVec3(("lightPositions[" + idx + "]").c_str(),
                                LIGHT_POSITIONS[i].x, LIGHT_POSITIONS[i].y, LIGHT_POSITIONS[i].z);
            floorShader.setVec3(("lightColors[" + idx + "]").c_str(),
                                LIGHT_COLORS[i].x, LIGHT_COLORS[i].y, LIGHT_COLORS[i].z);
        }

        glBindVertexArray(planeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_gamma ? floorSRGB : floorLinear);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        lampShader.use();
        lampShader.setMat4("view", view);
        lampShader.setMat4("projection", projection);
        glBindVertexArray(cubeVAO);
        for (int i = 0; i < 4; ++i)
        {
            lampShader.setVec3("lightColor", LIGHT_COLORS[i].x, LIGHT_COLORS[i].y, LIGHT_COLORS[i].z);
            glm::mat4 lampModel(1.0f);
            lampModel = glm::translate(lampModel, LIGHT_POSITIONS[i]);
            lampModel = glm::scale(lampModel, glm::vec3(0.1f));
            lampShader.setMat4("model", lampModel);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteTextures(1, &floorLinear);
    glDeleteTextures(1, &floorSRGB);
    glfwTerminate();
    return 0;
}

unsigned int loadTexture(const char* path, bool srgb)
{
    unsigned int textureID = 0;
    glGenTextures(1, &textureID);

    int width = 0, height = 0, nrComponents = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        GLint internal = GL_RGB;
        if (nrComponents == 1)
        {
            format = GL_RED;
            internal = GL_RED;
        }
        else if (nrComponents == 4)
        {
            format = GL_RGBA;
            internal = srgb ? GL_SRGB_ALPHA : GL_RGBA;
        }
        else if (srgb)
        {
            internal = GL_SRGB;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0,
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

    auto printMode = []() {
        std::cout << (g_gamma
                          ? "[gamma] ON  (sRGB tex, 1/d^2, pow 1/2.2)\n"
                          : "[gamma] OFF (RGB tex, 1/d, raw output)\n");
    };

    if (edge(window, GLFW_KEY_1, g_key1WasDown))
    {
        g_gamma = false;
        printMode();
    }
    if (edge(window, GLFW_KEY_2, g_key2WasDown))
    {
        g_gamma = true;
        printMode();
    }
    if (edge(window, GLFW_KEY_SPACE, g_keySpaceWasDown))
    {
        g_gamma = !g_gamma;
        printMode();
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
