# 实例化 Instancing（Day 10）

对应 LearnOpenGL「[实例化](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/10%20Instancing/)」：网格相同、只是位置 / 缩放 / 旋转不同时，不要循环一万次 `glDraw*`。把每份的变换放进缓冲，**一次绘制调用**画出全部实例。

| 文档 | 内容 |
|------|------|
| [Day09 几何着色器](../Day09/几何着色器GeometryShader.md) | 一个图元进、可以多个图元出 |
| **本文档** | `gl_InstanceID`、实例数组、`glVertexAttribDivisor`、小行星带 |
| 后续 | 抗锯齿 |

**工程对照**：当前 `main.cpp` 是小行星带 Demo。中央一颗 `planet.obj`，周围 `rock.obj` 撒成圆环。`1` 用 `glDrawElementsInstanced` 一次画 10000 颗（默认）；`2` 循环 500 次普通 `Draw`，用来对比 draw call。

**前置**：VAO / `glVertexAttribPointer`、模型加载（Assimp）、MVP。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Instancing | **实例化** | 同一份网格画很多份，每份只换变换（或颜色） |
| Draw call | **绘制调用** | 一次 `glDraw*`。调用次数多会在 CPU 和驱动之间反复切换 |
| `gl_InstanceID` | **实例编号** | 顶点着色器内建整数：当前是第几份，从 0 起 |
| Instanced array | **实例数组** | 顶点属性里放「每个实例一份」的数据，例如 model 矩阵 |
| `glVertexAttribDivisor` | **属性步进** | `0` 每个顶点更新；`1` 每个实例更新 |
| `glDrawElementsInstanced` | **实例化索引绘制** | 最后多一个 `instanceCount`，内部相当于循环 instanceCount 次，但不反复走 CPU |
| Asteroid belt | **小行星带** | 教程场景：行星在中间，岩石围成一圈 |

---

## 1. 为什么要实例化

循环画 N 个相同的石头：

```cpp
for (unsigned int i = 0; i < amount; ++i)
{
    shader.setMat4("model", modelMatrices[i]);
    rock.Draw(shader);   // 一次 draw call
}
```

网格、贴图、着色器都一样，变的只有 `model`。N 到几千时，瓶颈往往是 **draw call 次数**，不是顶点数。

实例化：网格仍只存一份，N 份变换放进另一块缓冲，GPU 按实例编号去取。CPU 只提交一次绘制。

---

## 2. `gl_InstanceID`：先理解「第几份」

顶点着色器里有一个只读整数 `gl_InstanceID`。`glDrawArraysInstanced(..., N)` 时，第 0 份全是 0，第 1 份全是 1，直到 N-1。

教程入门：100 个四边形，偏移放在 uniform 数组里，用 ID 当下标：

```glsl
layout (location = 0) in vec2 aPos;
uniform vec2 offsets[100];

void main()
{
    vec2 offset = offsets[gl_InstanceID];
    gl_Position = vec4(aPos + offset, 0.0, 1.0);
}
```

```cpp
glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 100);
```

uniform 数组有大小上限，一万份矩阵放不进去。小行星带改用**实例数组**。

---

## 3. 实例数组与 `glVertexAttribDivisor`

把「每个实例一份」的数据做成顶点属性，但告诉驱动：**不要每个顶点换一次，每个实例换一次**。

```cpp
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
glVertexAttribDivisor(2, 1);   // location 2：每个实例更新一次
glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 100);
```

| divisor | 这个属性何时换 |
|---------|----------------|
| 0（默认） | 每个顶点。位置、法线、UV 都是这种 |
| 1 | 每个实例。小行星的 model 矩阵是这种 |
| k>1 | 每 k 个实例换一次（本 Demo 不用） |

`mat4` 在属性里占 **4 个 vec4**，也就是 4 个 location。本工程从 location 3 起：

```
location 3 = 矩阵第 0 列
location 4 = 第 1 列
location 5 = 第 2 列
location 6 = 第 3 列
```

四个 location 都要 `Divisor(..., 1)`。`Mesh` 里 3、4 原本是切线、副切线，实例化 Demo 会盖掉它们；小行星只采样漫反射，不做法线贴图。

顶点着色器：

```glsl
layout (location = 3) in mat4 aInstanceMatrix;

void main()
{
    gl_Position = projection * view * aInstanceMatrix * vec4(aPos, 1.0);
}
```

`view` / `projection` 所有石头共用，仍走普通 uniform（或 Day08 的 UBO）。`model` 不再是 uniform。

---

## 4. 小行星带怎么摆

每颗石头一份 `mat4`：先平移到圆环上，再随机缩一点、转一个角度。

```
x = sin(θ) * radius + 随机偏移
y = 较小的随机高度
z = cos(θ) * radius + 随机偏移
```

`θ = i / N * 2π`，石头沿圆周均匀铺开。半径 50，中心是放大 4 倍的行星。远平面要够大（本 Demo `400`），否则圆环会被裁掉。

矩阵一次性算好，`GL_STATIC_DRAW` 上传。石头不转圈，缓冲不用每帧改。若以后要公转，改成 `GL_DYNAMIC_DRAW` 再 `glBufferSubData`。

---

## 5. 和本工程怎么接

### 5.1 资源

```
Resource/TestLoadModel/planet.obj + planet_Quom1200.png
Resource/TestLoadModel/rock.obj   + Rock-Texture-Surface.jpg
```

CMake POST_BUILD 会同步整个 `Resource/`。

### 5.2 画面与按键

相机在 `(0, 8, 55)`，看向原点。WASD、鼠标、Esc 与前面几章相同。

| 键 | 画面 |
|----|------|
| `1`（默认） | `glDrawElementsInstanced`，10000 颗石头，每个 mesh 一次 draw call |
| `2` | 循环 500 次 `rock.Draw`，每次 `setMat4("model")` |

模式 2 必须用带 `uniform mat4 model` 的 `planet.vs`。若仍用 `asteroid.vs`，非实例化绘制的 `gl_InstanceID` 恒为 0，所有石头都会用实例缓冲里第一份矩阵。

### 5.3 初始化（对照 `main.cpp`）

1. 加载 `planet.obj` / `rock.obj`。
2. CPU 生成 `ASTEROID_COUNT` 份矩阵，上传到 `instanceVBO`。
3. `setupRockInstanceAttribs`：对 rock 的每个 mesh VAO，把 location 3~6 指到这份缓冲，`Divisor = 1`。
4. 行星：`translate(0,-3,0)` 再 `scale(4)`，普通 `Draw`。
5. 石头：`rock.DrawInstanced(asteroidShader, ASTEROID_COUNT)`。

相关文件：

| 文件 | 作用 |
|------|------|
| `src/cppfile/main.cpp` | 圆环矩阵、实例 VBO、按键切换 |
| `shaders/asteroid.vs` / `.fs` | 实例矩阵当 model |
| `shaders/planet.vs` / `.fs` | 行星；模式 2 的石头也用它 |
| `Mesh.h` | `DrawInstanced`、`GetVAO` |
| `Model.h` | `DrawInstanced` 转给每个 mesh |

`Mesh::Draw` 设置的采样器名是 `material.texture_diffuse1`，片元着色器必须用这个名字。

---

## 6. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 只有一颗石头 / 全叠在一起 | 忘了 `glVertexAttribDivisor(..., 1)`，或非实例化绘制却读 `aInstanceMatrix` |
| 石头拉伸、碎掉 | mat4 四个 location 的 stride / offset 写错；stride 应是 `sizeof(glm::mat4)` |
| 行星或石头是黑的 | 采样器名不是 `material.texture_diffuse1`；或 obj 旁的 png/jpg 没拷到构建目录 |
| 圆环缺一块、远的石头消失 | 远平面太小。本 Demo 用 400 |
| 切线、法线贴图乱了 | location 3~6 盖掉了 Mesh 的 Tangent / Bitangent。本 Demo 只用漫反射 |
| 模式 2 所有石头同位置 | 用了 `asteroid.vs`。应改用带 `uniform model` 的着色器 |

---

## 7. 小练习

1. `glVertexAttribDivisor(loc, 0)` 和 `1` 各表示属性隔多久换一次？
2. 为什么 `mat4` 要占 4 个 location？本工程为什么从 3 开始，而不是 0？
3. 模式 2 若继续用 `asteroid.vs`，`gl_InstanceID` 是多少？石头会怎样？
4. uniform 数组能不能代替 10000 份矩阵？卡在什么上限上？
5. （思考）石头若每帧绕行星公转，实例缓冲该用 `GL_STATIC_DRAW` 还是 `GL_DYNAMIC_DRAW`？

---

## 8. 阅读顺序与下一步

```
Day09 几何着色器
  ↓
Day10 实例化（本文）← 一份网格，很多次绘制合成一次
  ↓
抗锯齿
```

---

## 9. 参考文献

1. LearnOpenGL CN — [实例化](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/10%20Instancing/)
2. LearnOpenGL EN — [Instancing](https://learnopengl.com/Advanced-OpenGL/Instancing)
3. 本工程 `Resource/TestLoadModel/planet.obj`、`rock.obj`
4. 本工程 `src/headfile/Mesh.h` — `DrawInstanced` / `glVertexAttribDivisor`
