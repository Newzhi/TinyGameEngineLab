#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// 摄像机移动方向（供 ProcessKeyboard 使用）
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

// FPS 摄像机：封装位置、朝向、欧拉角与 View 矩阵计算
class Camera {
public:
    // 摄像机属性
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // 欧拉角
    float Yaw;
    float Pitch;

    // 移动 / 鼠标 / 滚轮参数
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    // 构造函数：初始化位置、上方向、yaw/pitch，并计算 Front/Right
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
           glm::vec3 up      = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw         = -90.0f,
           float pitch       = 0.0f)
        : Position(position),
          WorldUp(up),
          Yaw(yaw),
          Pitch(pitch),
          Front(glm::vec3(0.0f, 0.0f, -1.0f)),
          MovementSpeed(2.5f),
          MouseSensitivity(0.05f),
          Zoom(45.0f),
          firstMouse(true),
          lastX(0.0f),
          lastY(0.0f)
    {
        updateCameraVectors();
    }

    // 析构函数：Camera 不持有 GPU/堆资源，无需特殊释放
    ~Camera() = default;

    // 返回 View 矩阵（每帧传给 Shader 的 view uniform）
    glm::mat4 GetViewMatrix() const
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // 键盘 WASD：根据方向平移 Position
    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;
        if (direction == FORWARD)
            Position += Front * velocity;
        if (direction == BACKWARD)
            Position -= Front * velocity;
        if (direction == LEFT)
            Position -= Right * velocity;
        if (direction == RIGHT)
            Position += Right * velocity;
    }

    // 鼠标移动：传入屏幕坐标，内部计算偏移并更新 yaw/pitch
    void ProcessMouseMovement(double xpos, double ypos, bool constrainPitch = true)
    {
        if (firstMouse)
        {
            lastX = static_cast<float>(xpos);
            lastY = static_cast<float>(ypos);
            firstMouse = false;
            return;
        }

        float xoffset = static_cast<float>(xpos) - lastX;
        float yoffset = lastY - static_cast<float>(ypos);
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);

        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw   += xoffset;
        Pitch += yoffset;

        if (constrainPitch)
        {
            if (Pitch > 89.0f)
                Pitch = 89.0f;
            if (Pitch < -89.0f)
                Pitch = -89.0f;
        }

        updateCameraVectors();
    }

    // 重置首帧鼠标标记（例如切换光标模式后调用）
    void ResetMouseFlag()
    {
        firstMouse = true;
    }

    // 滚轮：缩放视野 FOV（Zoom）
    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= yoffset;
        if (Zoom < 1.0f)
            Zoom = 1.0f;
        if (Zoom > 45.0f)
            Zoom = 45.0f;
    }

private:
    bool  firstMouse;
    float lastX;
    float lastY;

    // 由 Yaw/Pitch 更新 Front、Right、Up
    void updateCameraVectors()
    {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);

        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }
};

#endif // CAMERA_H
