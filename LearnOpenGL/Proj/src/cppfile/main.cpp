// GLAD：在调用任何 OpenGL 函数之前，必须先加载 OpenGL 函数指针
// 注意：glad.h 必须在 glfw3.h 之前 include，避免头文件冲突
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../headfile/Shader.h"
#include "../headfile/Camera.h"
#include "../headfile/Model.h"

#include <iostream>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void applySimpleDirLight(const Shader& shader, const Camera& camera);

// 绕平面 z = mirrorZ、法线 +Z 的反射矩阵：P' = T(z)·S(1,1,-1)·T(-z)·P
glm::mat4 makePlanarReflectionMatrix(float mirrorZ);
void drawFloor(const Shader& shader, unsigned int vao, const glm::mat4& model);
void drawBackpacks(Shader& shader, Model& backpack,
                   const std::vector<glm::vec3>& positions, float scale,
                   const glm::mat4& preTransform);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 100.0f;

// 镜子所在平面：z = MIRROR_Z，朝向 +Z（相机从 z> MIRROR_Z 一侧看过去）
const float MIRROR_Z = -3.0f;

unsigned int g_ScreenWidth = SCR_WIDTH;
unsigned int g_ScreenHeight = SCR_HEIGHT;

Camera* g_camera = nullptr;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// ---------------------------------------------------------------------------
// Part4 Day02：平面镜子（Stencil Mirror）
//
// 流程：
//   1) 先画真实世界（地板 + 背包），不写模板
//   2) 画镜面四边形：只写入模板 = 1（关颜色 / 关深度写入）
//   3) 仅在模板 == 1 处，用反射矩阵再画一遍场景 → 镜中倒影
//   4) 半透明镜面 + 镜框，让镜子「有玻璃感」
//
// 注意：本 Demo 把所有物体放在镜子前方，避免镜后物体被错误反射；
//       完整实现还需裁剪平面（gl_ClipDistance）。
// ---------------------------------------------------------------------------

glm::mat4 makePlanarReflectionMatrix(float mirrorZ)
{
    glm::mat4 m(1.0f);
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, mirrorZ));
    m = glm::scale(m, glm::vec3(1.0f, 1.0f, -1.0f));
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, -mirrorZ));
    return m;
}

void applySimpleDirLight(const Shader& shader, const Camera& camera)
{
    shader.setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);

    shader.setVec3("dirLight.direction", -0.3f, -1.0f, -0.4f);
    shader.setVec3("dirLight.ambient",  0.35f, 0.35f, 0.35f);
    shader.setVec3("dirLight.diffuse",  0.85f, 0.85f, 0.85f);
    shader.setVec3("dirLight.specular", 0.4f, 0.4f, 0.4f);

    for (int i = 0; i < 4; ++i)
    {
        std::string base = "pointLights[" + std::to_string(i) + "]";
        shader.setVec3(base + ".position", 0.0f, 0.0f, 0.0f);
        shader.setVec3(base + ".ambient",  0.0f, 0.0f, 0.0f);
        shader.setVec3(base + ".diffuse",  0.0f, 0.0f, 0.0f);
        shader.setVec3(base + ".specular", 0.0f, 0.0f, 0.0f);
        shader.setFloat(base + ".constant",  1.0f);
        shader.setFloat(base + ".linear",    0.09f);
        shader.setFloat(base + ".quadratic", 0.032f);
    }
    shader.setVec3("spotLight.position",  0.0f, 0.0f, 0.0f);
    shader.setVec3("spotLight.direction", 0.0f, -1.0f, 0.0f);
    shader.setVec3("spotLight.ambient",   0.0f, 0.0f, 0.0f);
    shader.setVec3("spotLight.diffuse",   0.0f, 0.0f, 0.0f);
    shader.setVec3("spotLight.specular",  0.0f, 0.0f, 0.0f);
    shader.setFloat("spotLight.cutOff",      std::cos(glm::radians(12.5f)));
    shader.setFloat("spotLight.outerCutOff", std::cos(glm::radians(17.5f)));
    shader.setFloat("spotLight.constant",  1.0f);
    shader.setFloat("spotLight.linear",    0.09f);
    shader.setFloat("spotLight.quadratic", 0.032f);
}

void drawFloor(const Shader& shader, unsigned int vao, const glm::mat4& model)
{
    shader.setMat4("model", model);
    shader.setVec3("uColor", 0.28f, 0.30f, 0.32f);
    shader.setFloat("uAlpha", 1.0f);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void drawBackpacks(Shader& shader, Model& backpack,
                   const std::vector<glm::vec3>& positions, float scale,
                   const glm::mat4& preTransform)
{
    for (const glm::vec3& pos : positions)
    {
        glm::mat4 model(1.0f);
        model = glm::translate(model, pos);
        model = glm::scale(model, glm::vec3(scale));
        // 反射遍：先把物体变到镜中坐标，再乘自身 TRS
        model = preTransform * model;
        shader.setMat4("model", model);
        backpack.Draw(shader);
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

    GLFWwindow* window = glfwCreateWindow(
        SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Stencil Mirror", NULL, NULL);
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
    glEnable(GL_STENCIL_TEST);
    glEnable(GL_CULL_FACE);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    Camera camera(glm::vec3(0.0f, 1.4f, 4.5f));
    g_camera = &camera;

    Shader modelShader("shaders/model.vs", "shaders/model.fs");
    Shader colorShader("shaders/outline.vs", "shaders/outline.fs");

    Model backpack("Resource/TestLoadModel/backpack.obj");

    // 地板：y = -0.5 的 XZ 平面（绕序保证法线 +Y，配合 Cull Back）
    float floorVertices[] = {
         6.0f, -0.5f,  4.0f,
        -6.0f, -0.5f, -6.0f,
        -6.0f, -0.5f,  4.0f,

         6.0f, -0.5f,  4.0f,
         6.0f, -0.5f, -6.0f,
        -6.0f, -0.5f, -6.0f
    };

    // 镜面：竖直放在 z = MIRROR_Z，面向 +Z
    // 注意绕序：从 +Z 看过去为 CCW，配合默认 Cull Back
    float mirrorVertices[] = {
        // 玻璃（内框）
        -1.6f, 0.0f, 0.0f,
         1.6f, 0.0f, 0.0f,
         1.6f, 2.4f, 0.0f,

        -1.6f, 0.0f, 0.0f,
         1.6f, 2.4f, 0.0f,
        -1.6f, 2.4f, 0.0f
    };

    // 镜框：比玻璃略大一圈，稍后画在玻璃外围（用两个四边形差不太方便，
    // 这里直接画一整块大框，再在上面「挖」玻璃区域——实际用前后两层：
    // 先画大框不透明，玻璃区域会被后续半透明覆盖。简化：四条边四边形。）
    float frameVertices[] = {
        // 底
        -1.85f, -0.12f, 0.0f,  1.85f, -0.12f, 0.0f,  1.85f,  0.00f, 0.0f,
        -1.85f, -0.12f, 0.0f,  1.85f,  0.00f, 0.0f, -1.85f,  0.00f, 0.0f,
        // 顶
        -1.85f,  2.40f, 0.0f,  1.85f,  2.40f, 0.0f,  1.85f,  2.52f, 0.0f,
        -1.85f,  2.40f, 0.0f,  1.85f,  2.52f, 0.0f, -1.85f,  2.52f, 0.0f,
        // 左
        -1.85f,  0.00f, 0.0f, -1.60f,  0.00f, 0.0f, -1.60f,  2.40f, 0.0f,
        -1.85f,  0.00f, 0.0f, -1.60f,  2.40f, 0.0f, -1.85f,  2.40f, 0.0f,
        // 右
         1.60f,  0.00f, 0.0f,  1.85f,  0.00f, 0.0f,  1.85f,  2.40f, 0.0f,
         1.60f,  0.00f, 0.0f,  1.85f,  2.40f, 0.0f,  1.60f,  2.40f, 0.0f
    };

    unsigned int floorVAO = 0, floorVBO = 0;
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    unsigned int mirrorVAO = 0, mirrorVBO = 0;
    glGenVertexArrays(1, &mirrorVAO);
    glGenBuffers(1, &mirrorVBO);
    glBindVertexArray(mirrorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mirrorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mirrorVertices), mirrorVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    unsigned int frameVAO = 0, frameVBO = 0;
    glGenVertexArrays(1, &frameVAO);
    glGenBuffers(1, &frameVBO);
    glBindVertexArray(frameVAO);
    glBindBuffer(GL_ARRAY_BUFFER, frameVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(frameVertices), frameVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // 全部放在镜子前方（z > MIRROR_Z），避免镜后几何被错误翻到镜前
    std::vector<glm::vec3> objectPositions = {
        glm::vec3(-1.0f, 0.0f, -0.2f),
        glm::vec3( 1.1f, 0.0f, -1.2f)
    };
    const float objectScale = 0.5f;

    const glm::mat4 reflectionMat = makePlanarReflectionMatrix(MIRROR_Z);
    const glm::mat4 identity(1.0f);

    // 镜子自己的 model：挪到 z = MIRROR_Z
    glm::mat4 mirrorModel(1.0f);
    mirrorModel = glm::translate(mirrorModel, glm::vec3(0.0f, 0.0f, MIRROR_Z));

    std::cout << "Stencil Mirror Demo\n"
              << "  1) draw real scene\n"
              << "  2) write mirror shape into stencil\n"
              << "  3) draw reflected scene where stencil == 1\n"
              << "  4) translucent glass + frame\n"
              << "  WASD + mouse, ESC quit\n";

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        float aspect = static_cast<float>(g_ScreenWidth) / static_cast<float>(g_ScreenHeight);
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), aspect, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);

        // ================================================================
        // 1) 真实场景：不写模板
        // ================================================================
        glStencilMask(0x00);
        glCullFace(GL_BACK);

        colorShader.use();
        colorShader.setMat4("view", view);
        colorShader.setMat4("projection", projection);
        drawFloor(colorShader, floorVAO, identity);

        modelShader.use();
        applySimpleDirLight(modelShader, camera);
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projection);
        modelShader.setFloat("material.shininess", 32.0f);
        drawBackpacks(modelShader, backpack, objectPositions, objectScale, identity);

        // ================================================================
        // 2) 镜面写入模板 = 1（不写颜色、不写深度）
        //    深度不写：镜中物体才能画进「洞」里；前方物体已有深度，会正确挡住倒影
        // ================================================================
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_FALSE);

        colorShader.use();
        colorShader.setMat4("view", view);
        colorShader.setMat4("projection", projection);
        colorShader.setMat4("model", mirrorModel);
        colorShader.setVec3("uColor", 1.0f, 1.0f, 1.0f);
        colorShader.setFloat("uAlpha", 1.0f);
        glBindVertexArray(mirrorVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);

        // ================================================================
        // 3) 只在模板 == 1 处画反射场景
        //    反射矩阵会翻转绕序 → 临时 Cull Front，避免画到背面
        // ================================================================
        glStencilFunc(GL_EQUAL, 1, 0xFF);
        glStencilMask(0x00);
        glCullFace(GL_FRONT);

        colorShader.use();
        colorShader.setMat4("view", view);
        colorShader.setMat4("projection", projection);
        drawFloor(colorShader, floorVAO, reflectionMat);

        modelShader.use();
        applySimpleDirLight(modelShader, camera);
        modelShader.setMat4("view", view);
        modelShader.setMat4("projection", projection);
        modelShader.setFloat("material.shininess", 32.0f);
        drawBackpacks(modelShader, backpack, objectPositions, objectScale, reflectionMat);

        glCullFace(GL_BACK);

        // ================================================================
        // 4) 半透明玻璃 + 不透明镜框（不再改模板）
        // ================================================================
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glStencilMask(0x00);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // 玻璃略写入深度，避免后续乱序；用较小 alpha 保留倒影
        colorShader.use();
        colorShader.setMat4("view", view);
        colorShader.setMat4("projection", projection);
        colorShader.setMat4("model", mirrorModel);
        colorShader.setVec3("uColor", 0.55f, 0.70f, 0.85f);
        colorShader.setFloat("uAlpha", 0.18f);
        glBindVertexArray(mirrorVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisable(GL_BLEND);

        colorShader.setVec3("uColor", 0.15f, 0.12f, 0.10f);
        colorShader.setFloat("uAlpha", 1.0f);
        colorShader.setMat4("model", mirrorModel);
        glBindVertexArray(frameVAO);
        glDrawArrays(GL_TRIANGLES, 0, 24);

        // 恢复状态
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_camera = nullptr;
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteVertexArrays(1, &mirrorVAO);
    glDeleteBuffers(1, &mirrorVBO);
    glDeleteVertexArrays(1, &frameVAO);
    glDeleteBuffers(1, &frameVBO);
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
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
