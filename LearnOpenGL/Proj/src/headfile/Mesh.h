#ifndef LEARN_OPENGL_PROJ_MESH_H
#define LEARN_OPENGL_PROJ_MESH_H

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"

#include <cstddef>   // offsetof
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 单个顶点：位置 + 法线 + UV（内存连续，便于整块上传到 VBO）
// ---------------------------------------------------------------------------
struct Vertex {
    glm::vec3 Position;   // 位置
    glm::vec3 Normal;     // 法线（光照）
    glm::vec2 TexCoords;  // 纹理坐标
};

// ---------------------------------------------------------------------------
// 一张已上传到 GPU 的纹理：OpenGL 纹理 id + 类型名
// type 约定："texture_diffuse" / "texture_specular"（后面拼序号 1,2,...）
// ---------------------------------------------------------------------------
struct Texture {
    unsigned int id;   // glGenTextures 得到的纹理对象
    std::string type;  // 贴图种类，供 Draw 时拼 uniform 名
};

// ---------------------------------------------------------------------------
// Mesh：一个可绘制的网格 = 顶点数组 + 索引 + 若干贴图 + VAO/VBO/EBO
// Assimp 的一个 aiMesh 最终会转成我们的一个 Mesh 实例（下一节 Model 类）
// ---------------------------------------------------------------------------
class Mesh {
public:
    // ----- CPU 侧网格数据（构造后也可只读访问）-----
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture>      textures;

    // 接收顶点 / 索引 / 贴图，立刻在 GPU 上建好缓冲
    Mesh(std::vector<Vertex> vertices,
         std::vector<unsigned int> indices,
         std::vector<Texture> textures)
    {
        this->vertices = std::move(vertices);
        this->indices  = std::move(indices);
        this->textures = std::move(textures);
        setupMesh();
    }

    // 绑定贴图采样器后，用 EBO 画出整个网格
    // shader 需在外部 use()；本函数会设置 material.texture_diffuseN 等 int uniform
    void Draw(Shader& shader)
    {
        unsigned int diffuseNr  = 1;
        unsigned int specularNr = 1;

        for (unsigned int i = 0; i < textures.size(); i++)
        {
            // 第 i 张贴图绑到纹理单元 i
            glActiveTexture(GL_TEXTURE0 + i);

            // 按类型生成序号：texture_diffuse1, texture_diffuse2, texture_specular1, ...
            std::string number;
            const std::string& name = textures[i].type;
            if (name == "texture_diffuse")
                number = std::to_string(diffuseNr++);
            else if (name == "texture_specular")
                number = std::to_string(specularNr++);

            // 告诉 shader：名为 material.xxxN 的 sampler 使用纹理单元 i
            // （具体命名可按你的 FS 调整；教程示例带 material. 前缀）
            shader.setInt("material." + name + number, static_cast<int>(i));
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }
        glActiveTexture(GL_TEXTURE0);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT,
                       nullptr);
        glBindVertexArray(0);
    }

private:
    // ----- GPU 侧缓冲对象 -----
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;

    // 创建并配置 VAO / VBO / EBO，以及三个顶点属性指针
    void setupMesh()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // VBO：整块 Vertex 数组（结构体内存连续）
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(Vertex),
                     vertices.data(),
                     GL_STATIC_DRAW);

        // EBO：索引
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned int),
                     indices.data(),
                     GL_STATIC_DRAW);

        // location 0：位置 —— 偏移 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(0));

        // location 1：法线 —— 用 offsetof 算到 Normal 成员的字节偏移
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, Normal)));

        // location 2：UV
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsetof(Vertex, TexCoords)));

        glBindVertexArray(0);
    }
};

#endif // LEARN_OPENGL_PROJ_MESH_H
