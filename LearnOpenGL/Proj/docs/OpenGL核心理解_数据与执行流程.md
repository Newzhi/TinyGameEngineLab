# OpenGL 核心理解（数据与执行流程）

## 一、核心思想

OpenGL 可以理解为：

> CPU 准备数据 → GPU 读取数据 → Shader 处理数据 → 输出到屏幕

------------------------------------------------------------------------

## 二、关键数据

### 1. Vertex（顶点数据）

``` cpp
struct Vertex{
    float x,y,z;
    float r,g,b;
};
```

描述模型的顶点属性，例如位置、颜色、法线、UV 等。

### 2. Shader 源码

GPU 使用的 GLSL 程序，包括：

-   Vertex Shader：处理每个顶点
-   Fragment Shader：处理每个像素

### 3. VBO（Vertex Buffer Object）

GPU 显存中的顶点缓冲。

作用：保存 Vertex 数据。

    CPU Vertex[]
          │
    glBufferData
          │
          ▼
    GPU VBO

### 4. VAO（Vertex Array Object）

保存**顶点布局**，而不是顶点数据。

例如：

    x y z r g b

    Position:
    Offset = 0

    Color:
    Offset = 3*sizeof(float)

    Stride = 6*sizeof(float)

### 5. Shader Object

编译后的 Vertex Shader / Fragment Shader。

### 6. Program

链接后的完整 GPU 程序。

    Vertex Shader
          +
    Fragment Shader
          │
          ▼
    Program

### 7. FrameBuffer

最终所有像素写入的位置，SwapBuffers 后显示到屏幕。

------------------------------------------------------------------------

## 三、执行流程

``` text
初始化 GLFW
    │
创建窗口
    │
初始化 GLAD
    │
编译 Shader
    │
创建 VBO/VAO
    │
上传 Vertex 数据
    │
配置顶点布局
    │
进入渲染循环
    │
清屏
使用 Program
绑定 VAO
Draw
SwapBuffers
```

------------------------------------------------------------------------

## 四、GPU 渲染流程

``` text
CPU Vertex[]
      │
      ▼
VBO
      │
      ▼
VAO（解析数据）
      │
      ▼
Vertex Shader
      │
      ▼
Rasterizer（光栅化）
      │
      ▼
Fragment Shader
      │
      ▼
FrameBuffer
      │
      ▼
Screen
```

------------------------------------------------------------------------

## 五、四个最重要的对象

  对象      作用
  --------- --------------
  Vertex    描述画什么
  VBO       保存数据
  VAO       描述数据格式
  Program   决定如何绘制

------------------------------------------------------------------------

# 补充：后续常见 OpenGL 对象

## EBO（Element Buffer Object）

保存索引数据，用于复用顶点。

    Vertex:
    0 1 2 3

    Index:
    0 1 2
    2 3 0

优点：

-   节省显存
-   避免重复顶点
-   使用 `glDrawElements()` 绘制

------------------------------------------------------------------------

## Texture（纹理）

保存图片数据。

流程：

    CPU 图片
        │
    glTexImage2D
        │
    GPU Texture
        │
    Fragment Shader 采样

Vertex 中通常增加 UV：

``` cpp
struct Vertex{
    Position;
    Color;
    UV;
};
```

------------------------------------------------------------------------

## Uniform

CPU 向 Shader 传递的全局参数。

例如：

-   MVP 矩阵
-   时间
-   光源颜色
-   摄像机位置
-   材质参数

特点：

-   一次设置，多次使用
-   一个 DrawCall 内所有顶点共享

例如：

``` glsl
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
```

------------------------------------------------------------------------

## Attribute

每个顶点独有的数据。

例如：

-   Position
-   Color
-   Normal
-   UV

来自 VBO。

------------------------------------------------------------------------

## Varying（in / out）

Vertex Shader 输出，Fragment Shader 输入。

GPU 会自动插值。

例如颜色渐变就是利用插值实现的。

------------------------------------------------------------------------

## MVP 矩阵

负责坐标变换：

    Model
        │
    View
        │
    Projection
        │
    Clip Space

Model：物体变换

View：相机变换

Projection：透视投影

------------------------------------------------------------------------

## Draw Call

一次绘制命令，例如：

``` cpp
glDrawArrays(...)
glDrawElements(...)
```

每调用一次，GPU 就开始执行一次完整渲染流程。

------------------------------------------------------------------------

## Render State

GPU 当前状态，例如：

-   Blend
-   Depth Test
-   Cull Face
-   Viewport

Program、VAO、Texture 等绑定都会影响 Render State。

------------------------------------------------------------------------

## 整体关系

``` text
CPU
│
├── Vertex[]
├── Index[]
├── Texture
├── Uniform
│
▼
GPU
│
├── VBO
├── EBO
├── VAO
├── Texture
├── Program
│
▼
Vertex Shader
│
▼
Rasterizer
│
▼
Fragment Shader
│
▼
FrameBuffer
│
▼
Screen
```
