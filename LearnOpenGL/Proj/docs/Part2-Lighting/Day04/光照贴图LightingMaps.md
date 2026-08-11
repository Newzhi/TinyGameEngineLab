# 光照贴图（Lighting Maps）（Day 04）

对应 LearnOpenGL Part 2「光照贴图」章节：**用纹理逐片元提供漫反射色与镜面强度**，让同一物体不同部位（木箱木板 vs 钢边框）对光照有不同反应。

| 文档 | 内容 |
|------|------|
| [Day02 基础光照与法线](../Day02/基础光照与法线.md) | Phong 三项、法线 |
| [Day03 材质 Materials](../Day03/材质Materials.md) | 整物体一套 `Material` 常数 |
| **本文档** | Diffuse Map / Specular Map / Emission Map、sampler 与纹理单元 |
| [Day05 投光物](../Day05/投光物LightCasters.md) | 平行光 / 点光衰减 / 聚光 |

> **公式说明**：公式用代码块书写，便于 Markdown 预览。

**前置**：已理解 Day03 的 Material / Light 拆分；Part1 纹理章节（加载、绑定、纹理单元）会立刻用上。

---

## 1. Day03 → Day04：问题出在哪？

Day03 给**整个物体**一套常数材质：

```
material.ambient / diffuse / specular = 固定 RGB
```

现实物体往往由多种表面组成，例如木箱：

| 部位 | 期望 |
|------|------|
| 木板 | 有木纹颜色，**几乎无高光** |
| 钢边框 | 偏金属色，**有镜面高光** |

一套常数无法同时满足——所以把「颜色 / 高光强度」从 **uniform 常数** 换成 **贴图采样**：

> **光照贴图 = 在 Phong 公式里，用纹理代替（部分）材质颜色。**

| | Day03 | Day04 |
|---|-------|-------|
| 漫反射色 | `material.diffuse`（vec3） | `texture(material.diffuse, uv)` |
| 环境光色 | `material.ambient`（vec3） | 通常 **复用漫反射贴图采样** |
| 镜面色/强度 | `material.specular`（vec3） | `texture(material.specular, uv)` |
| 反光度 | `shininess` 常数 | 仍常用一个 float（全物体共用） |
| 顶点数据 | 位置 + 法线 | 位置 + 法线 + **纹理坐标** |

---

## 2. 漫反射贴图（Diffuse Map）

### 2.1 是什么？

就是一张覆盖物体表面的图像，存的是各处的 **漫反射颜色**（木纹、油漆等）。  
原理与 Part1「纹理」完全相同，在光照语境里习惯叫 **Diffuse Map**。

教程常用示例：带钢边框的木箱贴图（container2）。

### 2.2 Material 怎么改？

把 `vec3 diffuse` 换成 `sampler2D`，并通常 **删掉单独的 ambient**：

```glsl
struct Material {
    sampler2D diffuse;  // 漫反射贴图
    vec3      specular; // 先仍可用常数；下一节再改成贴图
    float     shininess;
};

in vec2 TexCoords;
```

原因：环境光颜色几乎总等于漫反射颜色，不必再存一份。

采样用法：

```
texColor = texture(material.diffuse, TexCoords).rgb

ambient  = light.ambient * texColor
diffuse  = light.diffuse * diff * texColor
```

> **注意**：`sampler2D` 是 **不透明类型（Opaque Type）**，只能作为 **uniform**（或含它的 uniform struct）使用，不能当函数参数随便实例化，否则 GLSL 会报奇怪错误。

若仍想要与漫反射不同的环境光：可保留一个 `vec3 ambient`（整物体一个色），或再加一张环境光贴图——教程默认不这么做。

### 2.3 顶点侧要补什么？

1. 顶点数组增加 **纹理坐标**（每个顶点 `vec2`）；
2. VS 接收 `aTexCoords`，输出 `TexCoords` 给 FS；
3. 更新 VAO 的 `glVertexAttribPointer`（stride / offset）；
4. 加载图片 → 生成纹理对象 → 绑定到对应纹理单元。

顶点着色器骨架：

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos   = vec3(model * vec4(aPos, 1.0));
    Normal    = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

C++ 绑定漫反射贴图（约定纹理单元 0）：

```cpp
lightingShader.setInt("material.diffuse", 0);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, diffuseMap);
```

灯立方体一般 **不需要** 这套贴图，可继续用简单 `lamp` shader。

---

## 3. 镜面光贴图（Specular Map）

### 3.1 为什么需要？

只有漫反射贴图时：木板和钢边框都会按同一套 `specular` 出高光——木头高光过强，不真实。

若把 `specular` 整物体设为 `0`：钢边框也没高光了。

解决：再做一张贴图，用 **每个像素的亮度** 控制该处镜面强度（教程里多为黑白图）：

| 贴图像素 | 含义 |
|----------|------|
| 黑 `≈ (0,0,0)` | 几乎无高光（木板） |
| 灰 `≈ (0.5,0.5,0.5)` | 中等高光 |
| 白 `≈ (1,1,1)` | 强高光（钢边框） |

制作思路：从漫反射图出发，木纹涂黑、钢框保留并调对比度（Photoshop / GIMP 等）。

> 真实木头也有极弱高光；教学上常假设木头镜面为 0，突出对比。

### 3.2 Material 再升级

```glsl
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float     shininess;
};
```

绑定到 **另一个纹理单元**（例如 1）：

```cpp
lightingShader.setInt("material.specular", 1);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, specularMap);
```

片元计算：

```
texDiff = texture(material.diffuse,  TexCoords).rgb
texSpec = texture(material.specular, TexCoords).rgb

ambient  = light.ambient  * texDiff
diffuse  = light.diffuse  * diff * texDiff
specular = light.specular * spec * texSpec
```

彩色镜面贴图也能跑，但高光色主要由 **光源** 决定更自然，所以实践中镜面图多为 **灰度强度图**。

---

## 4. 完整 Phong（带双贴图）

符号同 Day02 / Day03：

```
N = normalize(Normal)
L = normalize(light.position - FragPos)
V = normalize(viewPos - FragPos)
R = reflect(-L, N)

diff = max(N · L, 0)
spec = pow(max(V · R, 0), material.shininess)

texDiff = texture(material.diffuse,  TexCoords).rgb
texSpec = texture(material.specular, TexCoords).rgb

ambient  = light.ambient  * texDiff
diffuse  = light.diffuse  * diff * texDiff
specular = light.specular * spec * texSpec

FragColor = vec4(ambient + diffuse + specular, 1.0)
```

片元着色器核心示例：

```glsl
void main()
{
    vec3 N = normalize(Normal);
    vec3 L = normalize(light.position - FragPos);
    vec3 V = normalize(viewPos - FragPos);

    vec3 texDiff = texture(material.diffuse,  TexCoords).rgb;
    vec3 texSpec = texture(material.specular, TexCoords).rgb;

    vec3 ambient = light.ambient * texDiff;

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = light.diffuse * diff * texDiff;

    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), material.shininess);
    vec3 specular = light.specular * spec * texSpec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

`shininess` 仍由 C++ `setFloat("material.shininess", 64.0f)` 之类上传（全箱共用一个锐度即可）。

---

## 5. 放射光贴图（Emission Map）（练习扩展）

**Emission**：物体自身「发光」的颜色，**不依赖** 灯光方向，直接加到最终色上。

常见用途：箱子上的灯带字、机器人眼睛、屏幕等。

```glsl
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    sampler2D emission;   // 可选
    float     shininess;
};

// ...
vec3 emission = texture(material.emission, TexCoords).rgb;
FragColor = vec4(ambient + diffuse + specular + emission, 1.0);
```

可再乘一个强度 uniform，或随时间滚动 UV，做出「流动发光」效果。

---

## 6. C++ / OpenGL 落地清单

按 LearnOpenGL 实现顺序自检：

| 步骤 | 内容 |
|------|------|
| 1 | 顶点数据：位置(3) + 法线(3) + UV(2)，stride = 8 floats |
| 2 | 物体 VAO：三个 `attrib`；灯 VAO 可只绑位置 |
| 3 | 加载 diffuse / specular（及可选 emission）纹理 |
| 4 | 每帧 draw 前：`setInt` 采样器 → 对应纹理单元 → `glBindTexture` |
| 5 | 设置 `light.*`、`viewPos`、`material.shininess`、MVP |
| 6 | FS 按第 4 节采样并合成 |

顶点布局示意：

```
| pos.xyz | normal.xyz | uv.xy |  →  8 floats / vertex
  loc 0      loc 1        loc 2
```

纹理单元约定（本笔记建议）：

| uniform | 纹理单元 |
|---------|----------|
| `material.diffuse` | 0 |
| `material.specular` | 1 |
| `material.emission`（若有） | 2 |

---

## 7. 数据流

```
C++:
  绑定 diffuse → TEXTURE0
  绑定 specular → TEXTURE1
  设置 light / viewPos / shininess / MVP
  Draw 箱子

VS:
  FragPos, Normal, TexCoords → FS

FS:
  从贴图取 texDiff / texSpec
  Phong：ambient + diffuse + specular（[+ emission]）
```

与 Day03 相比：**公式结构不变**，只是 `material.diffuse/specular` 的数据来源从常数变成 `texture(...)`。

---

## 8. 和当前项目代码的关系

当前工程已实现 **光照贴图 Demo**（`main.cpp` + `lightTest.vs/fs`）：

| 资源 / 代码 | 作用 |
|-------------|------|
| `Resource/Texture/container2.png` | 漫反射贴图 → `material.diffuse`（单元 0） |
| `Resource/Texture/container2_specular.png` | 镜面贴图 → `material.specular`（单元 1） |
| 顶点布局 | 位置 + 法线 + UV，stride = 8 floats |
| `loadTexture` | stb_image 加载 + mipmap |
| 灯立方体 | 仍用 `lamp` shader，绕箱子旋转 |

观察要点：木板区域几乎无高光；钢边框随灯/视角出现亮斑。WASD + 鼠标漫游。

重新配置 CMake 后会复制 `shaders/` 与 `Resource/`；若只改了 shader，也可手动复制到构建目录。

---

## 9. 小练习（对齐官网）

1. 改 `light.ambient / diffuse / specular`，观察木箱明暗与高光强弱变化。
2. 在 FS 里对镜面贴图采样结果取反：`1.0 - texSpec`，让木头亮、钢框暗（裂缝处可能仍有一点高光）。
3. 用**彩色**镜面贴图代替灰度图，感受高光颜色不自然之处。
4. 加入 **Emission Map**，让箱子表面字母/灯带自发光；可尝试滚动 UV。
5. （思考）为何 ambient 直接乘 `texDiff`，而不是再开一张 ambient 贴图？

---

## 10. 公式速查

```
N, L, V, R 同 Day03

texDiff = texture(diffuseMap,  uv).rgb
texSpec = texture(specularMap, uv).rgb

ambient  = light.ambient  * texDiff
diffuse  = light.diffuse  * max(N·L, 0) * texDiff
specular = light.specular * pow(max(V·R, 0), shininess) * texSpec
[+ emission = texture(emissionMap, uv).rgb]

FragColor = ambient + diffuse + specular [+ emission]
```

---

## 11. 阅读顺序与下一步

```
Day01 颜色与场景
  ↓
Day02 基础光照与法线
  ↓
Day03 材质（整物体常数 Material）
  ↓
Day04 光照贴图（本文）← Diffuse / Specular [/ Emission] Map
  ↓
Day05 投光物 ← 平行光 / 点光衰减 / 聚光
  ↓
多光源
```

之后还会遇到法线贴图、反射贴图等；投光物之后则是把多种光源合进同一场景。

---

## 12. 参考文献

1. LearnOpenGL CN — [光照贴图](https://learnopengl-cn.github.io/02%20Lighting/04%20Lighting%20maps/)
2. LearnOpenGL EN — [Lighting maps](https://learnopengl.com/Lighting/Lighting-maps)
3. LearnOpenGL CN — [材质](https://learnopengl-cn.github.io/02%20Lighting/03%20Materials/)
4. LearnOpenGL CN — [纹理](https://learnopengl-cn.github.io/01%20Getting%20started/06%20Textures/)（纹理单元与采样）
5. 《Part2 Day03 — 材质 Materials》— 常数材质与 Phong 分量相乘
