# 立方体贴图 Cubemaps（Day 06）

对应 LearnOpenGL「[立方体贴图](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/06%20Cubemaps/)」：把 **6 张 2D 图** 合成一张立方体纹理，用 **方向向量** 采样。最常用的是 **天空盒**，以及在此之上的 **环境映射**（反射 / 折射）。

| 文档 | 内容 |
|------|------|
| [Day01～Day04](../README.md) | 深度 / 模板 / 混合 / 面剔除 |
| [Day05 帧缓冲](../Day05/帧缓冲Framebuffers.md) | 离屏画布、后处理；动态环境贴图会再用到 FBO |
| **本文档** | `GL_TEXTURE_CUBE_MAP`、天空盒、深度优化、反射 / 折射 |
| 后续 | 高级数据、几何着色器、实例化…… |

**工程对照**：当前 `main.cpp` 为天空盒 + 环境映射 Demo。6 面加载为 cubemap；**箱子默认用反射方向采样这张 cubemap**（铬金属感）。地板仍是 2D 贴图。后画天空盒（`view` 去平移 + `xyww` + `GL_LEQUAL`）。`1` 切回贴图，`2` 反射，`3` 折射。

**前置**：2D 纹理加载、MVP、深度测试、面剔除（天空盒画的是立方体**内侧**）。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Cubemap / Cube map | **立方体贴图** | 一张纹理含 6 个面，用 3D 方向采样 |
| `GL_TEXTURE_CUBE_MAP` | **立方体纹理目标** | 绑定 / 采样立方体贴图时用这个，不是 `GL_TEXTURE_2D` |
| Face | **面** | 右/左/上/下/后/前 六张 2D 图 |
| `samplerCube` | **立方体采样器** | 片元着色器里对立方体贴图采样的类型 |
| Skybox | **天空盒** | 包住整个场景的大立方体，贴环境图，制造「周围很大」的错觉 |
| Environment mapping | **环境映射** | 用环境立方体贴图给物体反射 / 折射颜色 |
| Reflection | **反射** | `reflect(I, N)` 得到方向，再采 cubemap |
| Refraction | **折射** | `refract(I, N, eta)`，eta 为折射率之比 |
| Dynamic environment mapping | **动态环境贴图** | 每帧对物体 6 个方向离屏渲染，写入 cubemap（很贵） |

---

## 1. 立方体贴图是什么？

2D 纹理用 `vec2` UV 采样。  
立方体贴图把 6 张图贴在立方体的 6 个面上，用 **从中心指出去的方向 `vec3`** 采样：方向击中哪一面、哪一个纹素，就返回那个颜色。

```
          +Y (top)
             |
    -X ------+------ +X     （立方体中心为原点）
   (left)    |     (right)
            -Y (bottom)
             ±Z：前 / 后（约定见 §2）
```

要点：

- **方向的长度不重要**，只看朝哪边。  
- 若立方体中心在原点，**顶点位置本身就可以当采样方向**（天空盒常用这一招）。  
- 为什么合成一张而不是 6 个独立 2D 纹理：硬件按方向自动选面、面与面交界处可过滤，环境采样写起来也简单。

---

## 2. 创建与加载（关键代码）

和 2D 纹理一样先 `glGenTextures`，但绑定目标改成立方体：

```cpp
unsigned int textureID;
glGenTextures(1, &textureID);
glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
```

六个面各调用一次 `glTexImage2D`，`target` 换成对应面：

| 纹理目标 | 方位（官网表） |
|----------|----------------|
| `GL_TEXTURE_CUBE_MAP_POSITIVE_X` | 右 |
| `GL_TEXTURE_CUBE_MAP_NEGATIVE_X` | 左 |
| `GL_TEXTURE_CUBE_MAP_POSITIVE_Y` | 上 |
| `GL_TEXTURE_CUBE_MAP_NEGATIVE_Y` | 下 |
| `GL_TEXTURE_CUBE_MAP_POSITIVE_Z` | 后 |
| `GL_TEXTURE_CUBE_MAP_NEGATIVE_Z` | 前 |

这些枚举值是连续的，可从 `POSITIVE_X` 起 `+ i` 循环。  
**faces 路径顺序必须与上表一致**，否则天空会对不上。

```cpp
unsigned int loadCubemap(const std::vector<std::string>& faces)
{
    unsigned int textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width = 0, height = 0, nrChannels = 0;
    // 立方体面不要和 2D 贴图混用同一套「垂直翻转」；
    // Model.h 的 TextureFromFile 会把 stbi_set_flip_vertically_on_load 设成 true，
    // 加载天空盒前应显式设为 false（与官网 skybox 资源配套）。
    stbi_set_flip_vertically_on_load(false);

    for (unsigned int i = 0; i < faces.size(); ++i)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, static_cast<GLint>(format), width, height, 0,
                         format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return textureID;
}

std::vector<std::string> faces = {
    "Resource/Texture/skybox/right.jpg",
    "Resource/Texture/skybox/left.jpg",
    "Resource/Texture/skybox/top.jpg",
    "Resource/Texture/skybox/bottom.jpg",
    "Resource/Texture/skybox/front.jpg",
    "Resource/Texture/skybox/back.jpg"
};
unsigned int cubemapTexture = loadCubemap(faces);
```

`WRAP_R` 是第三维（类似位置的 z）。面交界处用 `CLAMP_TO_EDGE`，避免缝。

片元着色器里用 `samplerCube`，第二参数是 `vec3`：

```glsl
in vec3 textureDir;
uniform samplerCube cubemap;

void main()
{
    FragColor = texture(cubemap, textureDir);
}
```

---

## 3. 天空盒

天空盒 = 包住场景的立方体 + 上面这张环境 cubemap。  
玩家始终在「盒子里」，但图是远景（山、云、星空），看起来环境比真实几何大得多。

### 3.1 用位置当「UV」

立方体若以原点为中心，顶点坐标就是从中心指出的方向 → 直接当 cubemap 采样向量，**不必另备 2D UV**。

顶点着色器（未优化版）：

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
```

片元着色器：

```glsl
#version 330 core
out vec4 FragColor;
in vec3 TexCoords;
uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
}
```

### 3.2 天空盒要跟着「看」，不要跟着「走」

观察矩阵里的**平移**会把天空盒跟着相机挪走，近处盒子穿帮。  
只保留旋转（左上 3×3），去掉位移：

```cpp
glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
```

投影照常用透视。model 一般是单位阵（立方体本身已在原点）。

### 3.3 先画天空 vs 后画天空

| 做法 | 说明 |
|------|------|
| 先画天空盒，关深度写入 | 简单；每个像素都跑天空 FS，浪费带宽 |
| **后画天空盒**（官网优化） | 先让场景填满深度；天空只画「没有物体」的像素 |

后画时天空是 1×1×1 的立方体，默认深度会很小，会盖住场景。解决：让透视除法后 **z = 1.0**（最远）：

```glsl
void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;   // 透视除法后 z = w/w = 1.0
}
```

同时深度函数改为 `GL_LEQUAL`（缓冲里已是 1.0，`GL_LESS` 会失败）：

```cpp
glDepthFunc(GL_LEQUAL);
// 画天空盒
glDepthFunc(GL_LESS);   // 画完改回默认
```

画天空盒时人在立方体**内部**，默认剔背面可能把内侧面剔掉。常见处理：`glCullFace(GL_FRONT)` 或暂时 `glDisable(GL_CULL_FACE)`，画完再恢复（见 Day04）。

### 3.4 每帧顺序（优化版）

```
1. 清颜色 + 深度；深度函数 GL_LESS
2. 画场景（箱子、模型……）  → 深度缓冲有物体
3. 去掉 view 的平移；glDepthFunc(GL_LEQUAL)
4. 画天空盒（shader 里 pos.xyww）
5. 恢复 DepthFunc / CullFace
```

### 3.5 天空盒绘制全过程（对照本工程 `main.cpp`）

前面几节讲的是「为什么」，这里按代码执行顺序串一遍「怎么画」。分为**初始化一次**和**每帧绘制**两部分。

#### 初始化阶段（只做一次）

**第 1 步：准备天空盒立方体的顶点。**  
只需要位置，不需要法线和 UV。立方体中心在原点、边长 2（坐标 ±1），共 36 个顶点（6 面 × 2 三角形 × 3 顶点）：

```cpp
float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    // ……共 36 行，见 main.cpp
};
```

大小其实无所谓：后面 `xyww` 会把深度固定到最远，去掉平移后相机永远在盒子正中心，盒子多大看起来都一样。

**第 2 步：建 VAO / VBO，只配一个属性。**

```cpp
glGenVertexArrays(1, &skyboxVAO);
glGenBuffers(1, &skyboxVBO);
glBindVertexArray(skyboxVAO);
glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
glEnableVertexAttribArray(0);   // location 0 = aPos，步长 3 个 float
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
```

**第 3 步：加载 6 张图成一张立方体贴图。**  
`faces` 的顺序必须是 右、左、上、下、后、前，对应 `GL_TEXTURE_CUBE_MAP_POSITIVE_X + i`（见 §2）：

```cpp
unsigned int cubemapTexture = loadCubemap(faces);
```

**第 4 步：编译天空盒着色器，告诉采样器用哪个纹理单元。**

```cpp
Shader skyboxShader("shaders/skybox.vs", "shaders/skybox.fs");
skyboxShader.use();
skyboxShader.setInt("skybox", 0);   // samplerCube skybox ← GL_TEXTURE0
```

#### 每帧绘制阶段

**第 5 步：清屏，先画场景。**  
深度函数保持默认 `GL_LESS`。箱子、地板画完后，深度缓冲里有物体的像素存的是它们的深度（< 1.0），没有物体的像素仍是清屏值 1.0。

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
// …… 画箱子、地板 ……
```

**第 6 步：深度函数改成 `GL_LEQUAL`。**

```cpp
glDepthFunc(GL_LEQUAL);
```

天空盒的深度会被强制为 1.0。空白像素的深度缓冲也是 1.0，用 `GL_LESS` 时 `1.0 < 1.0` 不成立，天空会一个像素都画不上；`GL_LEQUAL` 让「等于」也通过。

**第 7 步：构造去掉平移的 view 矩阵。**

```cpp
glm::mat4 skyView = glm::mat4(glm::mat3(view));
```

4×4 观察矩阵的平移在第 4 列。取左上 3×3（只剩旋转），再扩回 4×4，第 4 列变成 `(0,0,0,1)`。效果：转头时天空跟着转，走路时天空不动，好像无限远。

**第 8 步：设置 uniform，绑定立方体贴图，绘制。**

```cpp
skyboxShader.use();
skyboxShader.setMat4("view", skyView);
skyboxShader.setMat4("projection", projection);   // 投影矩阵和场景共用
glBindVertexArray(skyboxVAO);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);   // 注意是 CUBE_MAP，不是 2D
glDrawArrays(GL_TRIANGLES, 0, 36);
```

不需要 `model` 矩阵：天空盒本来就在原点，也不需要缩放。

**第 9 步：恢复深度函数。**

```cpp
glDepthFunc(GL_LESS);
```

不恢复的话下一帧的场景会用 `GL_LEQUAL` 做深度测试，共面物体可能出现闪烁。

#### 着色器里发生了什么

顶点着色器 `skybox.vs`：

```glsl
TexCoords = aPos;                                  // ① 位置直接当采样方向
vec4 pos = projection * view * vec4(aPos, 1.0);    // ② 只有旋转 + 投影
gl_Position = pos.xyww;                            // ③ 把 z 换成 w
```

- ① 立方体中心在原点，每个顶点坐标就是「从中心指向这个顶点」的方向；光栅化插值后，每个片元拿到的是「从相机看出去指向这个像素」的方向。  
- ③ 之后 GPU 做透视除法 `(x/w, y/w, z/w)`，z 变成 `w/w = 1.0`，也就是 NDC 里最远的深度。

片元着色器 `skybox.fs`：

```glsl
FragColor = texture(skybox, TexCoords);   // 用方向向量 vec3 采 samplerCube
```

GPU 根据方向哪个分量绝对值最大选中一个面（比如 `x` 最大且为正就选右面），再在该面上算出 2D 坐标取颜色。

#### 一个像素的完整命运

以屏幕上两个像素为例，走一遍深度测试：

| 像素 | 场景画完后深度缓冲 | 天空盒片元深度 | `GL_LEQUAL` 结果 | 最终颜色 |
|------|---------------------|----------------|------------------|----------|
| 箱子所在像素 | 0.35（箱子） | 1.0 | `1.0 <= 0.35` 不成立，丢弃 | 箱子 |
| 空白像素 | 1.0（清屏值） | 1.0 | `1.0 <= 1.0` 成立，写入 | 天空 |

所以天空盒只在「没被任何物体占据」的像素上运行片元着色器，这正是后画天空盒比先画省的地方（配合 Early-Z，被挡住的片元连 FS 都不跑）。

#### 容易踩的坑

| 现象 | 原因 |
|------|------|
| 天空完全不显示 | 忘了 `GL_LEQUAL`；或开了 `GL_BACK` 剔除把内侧面剔掉了 |
| 天空盖住所有物体 | 顶点着色器没写 `xyww`，天空深度很小 |
| 走动时天空靠近 / 远离 | 用了完整 `view`，没做 `mat4(mat3(view))` |
| 天空是黑的 | 绑成了 `GL_TEXTURE_2D`；或 cubemap 加载失败（看控制台） |
| 某面上下颠倒 / 位置错 | `faces` 顺序错，或加载时开了垂直翻转 |

---

## 4. 环境映射

天空盒不只是背景：整张环境图还可以给物体当「周围世界的颜色查询表」。

### 4.1 反射（本工程默认）

视线相对法线弹开，用反射方向采 cubemap → 镜子 / 铬金属感。  
**当前 Demo 启动时两个箱子就是这个模式**（`g_objectMode = 1`）。地板强制 `uMode = 0`，仍显示木纹，方便和反射箱对比。

片元着色器 `cubemapObject.fs`（`uMode == 1`）：

```glsl
vec3 I = normalize(FragPos - cameraPos);      // 从相机指向片元
vec3 R = reflect(I, normalize(Normal));
FragColor = vec4(texture(skybox, R).rgb, 1.0);
```

顶点着色器 `cubemapObject.vs` 把位置、法线变到世界空间（法线用法线矩阵，避免非均匀缩放把法线拉歪）：

```glsl
Normal = mat3(transpose(inverse(model))) * aNormal;
FragPos = vec3(model * vec4(aPos, 1.0));
```

CPU 侧每帧要做三件事，缺一不可：

```cpp
shader.setVec3("cameraPos", camera.Position.x, camera.Position.y, camera.Position.z);
shader.setInt("uMode", 1);                          // 走反射分支

glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture); // 与 setInt("skybox", 1) 对应
```

`skybox` 采样器初始化时绑在纹理单元 1，箱子的 2D 贴图在单元 0，两者不抢槽。反射和天空盒背景用的是**同一张** cubemap，所以箱子上看到的图案和周围天空能对上。转头时反射方向跟着变，这就是环境映射。

整物体纯反射会像一整块金属；真实模型常用 **反射贴图** 控制哪些区域、多强（官网练习：Assimp 里可用 `aiTextureType_AMBIENT` 槽位「骗」反射图）。

按键：`1` 普通贴图、`2` 反射（默认）、`3` 折射。

### 4.2 折射

光线换介质时弯折（斯涅尔定律）。GLSL：`refract(I, N, eta)`。  
`eta` = 入射介质折射率 / 出射介质折射率。空气→玻璃约为 `1.00 / 1.52`。

| 材质 | 折射率 |
|------|--------|
| 空气 | 1.00 |
| 水 | 1.33 |
| 冰 | 1.309 |
| 玻璃 | 1.52 |
| 钻石 | 2.42 |

```glsl
float ratio = 1.00 / 1.52;
vec3 I = normalize(Position - cameraPos);
vec3 R = refract(I, normalize(Normal), ratio);
FragColor = vec4(texture(skybox, R).rgb, 1.0);
```

简单立方体折射往往像放大镜；复杂模型（背包）更明显。物理上光线离开物体还应再折射一次，入门用**单面折射**即可。

### 4.3 动态环境贴图（和 Day05 的关系）

静态天空盒**不含**场景里其它会动的物体。  
若镜子里也要出现旁边的箱子：对反射体从 6 个方向把场景渲进 cubemap 的 6 个面（每面一个 FBO 颜色附件或 `glFramebufferTexture2D` 指定 cubemap 的某一个 face）。

代价：**每个反射物体每帧最多 6 次整场景**。游戏里多用静态 / 预烘焙 cubemap，动态只偶尔用或降低分辨率。

---

## 5. 和 2D 纹理、帧缓冲怎么一起记？

| | 2D 纹理 | 立方体贴图 | 帧缓冲颜色附件 |
|--|---------|------------|----------------|
| 采样坐标 | `vec2` UV | `vec3` 方向 | 后处理时仍是 `vec2` UV |
| 典型用途 | 模型表面 | 天空、反射、IBL | 整屏画面 / G-Buffer |
| 数据来源 | 文件 1 张 | 文件 6 张（或 6 次离屏） | GPU 画进去 |

天空盒优化用到 Day01 的 **透视除法与深度 = z/w**；内侧绘制用到 Day04 **面剔除**。

---

## 6. 和本工程怎么接

### 6.1 建议资源布局

把官网 skybox 六张图放到：

```
Resource/Texture/skybox/right.jpg
Resource/Texture/skybox/left.jpg
Resource/Texture/skybox/top.jpg
Resource/Texture/skybox/bottom.jpg
Resource/Texture/skybox/front.jpg
Resource/Texture/skybox/back.jpg
```

CMake POST_BUILD 会同步整个 `Resource/`。官方资源：[LearnOpenGL skybox](https://learnopengl.com/img/textures/skybox.zip)（与中文教程「可以在这里下载」为同一套）。

### 6.2 Demo 内容（当前 `main.cpp`）

1. `loadCubemap` 按 右/左/上/下/后/前 加载 `Resource/Texture/skybox/*.jpg`  
2. 先画两个箱子（默认反射天空盒）+ 地板（2D 贴图），填深度  
3. 后画天空盒：`mat4(mat3(view))`、`gl_Position = pos.xyww`、`glDepthFunc(GL_LEQUAL)`  
4. 数字键：`1` 箱子贴图、`2` 反射天空（默认）、`3` 折射（玻璃 eta）  

相关文件：

| 文件 | 作用 |
|------|------|
| `src/cppfile/main.cpp` | 加载 cubemap、双物体 + 天空盒 |
| `shaders/skybox.vs` / `skybox.fs` | 天空盒（位置当方向，`xyww`） |
| `shaders/cubemapObject.vs` / `cubemapObject.fs` | 箱子/地板；反射与折射 |
| `Resource/Texture/skybox/` | 六面 jpg |

### 6.3 排错 checklist

| 现象 | 排查 |
|------|------|
| 六面错乱 / 接缝 | `faces` 顺序是否与 POSITIVE_X… 表一致？ |
| 上下颠倒 | `stbi_set_flip_vertically_on_load` 是否与 2D 贴图冲突？ |
| 走动时天空跟着平移 | `view` 是否做成 `mat4(mat3(view))`？ |
| 天空盖住所有物体 | 是否 `xyww`？是否 `GL_LEQUAL`？ |
| 天空全黑 / 内侧被剔 | 是否在立方体内部却 `Cull BACK`？ |
| 反射方向怪 | `I` 是否世界空间？法线是否用了法线矩阵？ |

---

## 7. 小练习

1. 为何 cubemap 用方向采样，天空盒却能把 `aPos` 直接当 `TexCoords`？  
2. 去掉观察矩阵平移后，天空盒还会旋转吗？为什么？  
3. `gl_Position = pos.xyww` 如何让深度变成 1.0？为何深度函数要改成 `GL_LEQUAL`？  
4. 反射和折射的 `I` 向量方向约定是什么？  
5. （思考）动态 cubemap 和 Day05 FBO 如何接到同一个物体上？性能瓶颈在哪？  
6. （官网）给模型加载器加反射贴图，天空盒绑到第 4 个纹理单元。

---

## 8. 阅读顺序与下一步

```
Day05 帧缓冲
  ↓
Day06 立方体贴图（本文）← cubemap + 天空盒 + 环境映射
  ↓
高级数据 / 几何着色器 / 实例化 …
  ↓（更后）IBL、延迟着色里的环境项
```

---

## 9. 参考文献

1. LearnOpenGL CN — [立方体贴图](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/06%20Cubemaps/)  
2. LearnOpenGL EN — [Cubemaps](https://learnopengl.com/Advanced-OpenGL/Cubemaps)  
3. 《Part4 Day01 — 深度测试》— 透视除法后的 z、深度函数  
4. 《Part4 Day04 — 面剔除》— 从立方体内部绘制  
5. 《Part4 Day05 — 帧缓冲》— 动态环境贴图的离屏 6 面  
