# 材质（Materials）（Day 03）

对应 LearnOpenGL Part 2「材质」章节：**把物体表面属性从硬编码里拆出来**，用 Material / Light 结构体分别描述「物体怎么反射」和「灯怎么发光」，从而模拟塑料、金属、橡胶等不同外观。

| 文档 | 内容 |
|------|------|
| [Day01 颜色与场景](../Day01/颜色与光照场景搭建.md) | RGB 相乘、双立方体实验场 |
| [Day02 基础光照与法线](../Day02/基础光照与法线.md) | Ambient / Diffuse / Specular、法线、Phong 公式 |
| **本文档** | Material、光照三分量拆分、struct uniform、OpenGL/VRML 材质表 |
| [Day04 光照贴图](../Day04/光照贴图LightingMaps.md) | Diffuse / Specular / Emission Map |

> **公式说明**：公式用代码块书写，便于 Markdown 预览。

**前置**：已理解 Day02 的 Phong 三项，以及当前 Demo 里 `(ambient+diffuse+specular)*objectColor` 的写法。

---

## 1. Day02 → Day03：问题出在哪？

Day02 典型写法：

```glsl
ambient  = 0.1 * lightColor;
diffuse  = diff * lightColor;
specular = 0.5 * spec * lightColor;
result   = (ambient + diffuse + specular) * objectColor;
```

问题：

1. **物体只有一个颜色** `objectColor`，三项共用——现实中高光色往往偏白，底色可以是别的；
2. **强度写死在 shader 里**（`0.1`、`0.5`、`32`），换一种材质要改代码重编译；
3. **灯光也被简化成一个 `lightColor`**，无法表达「灯的漫反射成分强、镜面成分弱」这类差异。

材质章节要做的事：

> **物体属性**（Material）和 **灯光属性**（Light）分开传，Phong 公式改成「材质 × 灯光」的分量相乘。

| | Day02 | Day03 |
|---|-------|-------|
| 物体 | 单个 `objectColor` | `material.ambient/diffuse/specular` + `shininess` |
| 灯光 | 单个 `lightColor` | `light.ambient/diffuse/specular` |
| 强度 / 高光锐度 | 写死在 FS | 由材质 / 灯光 uniform 控制 |
| 换外观 | 改 shader 常量 | 只改 C++ 传入的材质数据 |

---

## 2. 材质是什么？（形象理解）

把物体想成「对三种光的回答方式」：

| 问题 | 材质字段 | 直觉 |
|------|----------|------|
| 环境底光下显什么色？ | `ambient` | 阴影里仍能看出的底色 |
| 被灯直射时主色是什么？ | `diffuse` | 平常说的「物体颜色」 |
| 高光斑是什么色、多尖？ | `specular` + `shininess` | 塑料高光偏白；有的材质高光几乎没有 |

类比：

- **粉笔 / 橡胶**：`specular` 很小 → 几乎无亮斑；
- **塑料**：`specular` 中等、偏白，`shininess` 中等；
- **金属感**（在简单 Phong 里近似）：`diffuse` 带金属色，`specular` 也偏金属色且 `shininess` 很大。

> 真正的金属 PBR 更复杂；本节仍是 Phong 材质参数，用来「看起来像」不同表面。

---

## 3. 灯光为什么也要拆成三项？

同一盏灯对三种反射的「供给」可以不同：

```
light.ambient   // 提供给环境光计算的光色（常较弱）
light.diffuse   // 提供给漫反射的主光色
light.specular  // 提供给高光的光色（常接近白或略偏灯色）
```

常见简化（LearnOpenGL 教程也常用）：

```
light.ambient  = lightColor * 0.2
light.diffuse  = lightColor * 0.5
light.specular = lightColor        // 高光用较亮的光
```

或三项都等于同一个 `lightColor`，再靠材质区分物体。

---

## 4. 带材质的 Phong 公式

沿用 Day02 符号（世界空间、单位向量）：

```
N = normalize(Normal)
L = normalize(lightPos - FragPos)
V = normalize(viewPos - FragPos)
R = reflect(-L, N)
```

### 4.1 Ambient

```
ambient = light.ambient * material.ambient
```

### 4.2 Diffuse

```
diff    = max(N · L, 0)
diffuse = light.diffuse * (diff * material.diffuse)
```

### 4.3 Specular

```
spec     = pow(max(V · R, 0), material.shininess)
specular = light.specular * (spec * material.specular)
```

### 4.4 合成

```
FragColor.rgb = ambient + diffuse + specular
FragColor.a   = 1.0
```

**注意**：这里**不再**统一乘一个 `objectColor`——颜色已经分别写在 `material.ambient/diffuse/specular` 里。

### 4.5 与 Day02 公式的对应

| Day02 | Day03 |
|-------|-------|
| `0.1 * lightColor` | `light.ambient * material.ambient` |
| `diff * lightColor` | `light.diffuse * diff * material.diffuse` |
| `0.5 * spec * lightColor` | `light.specular * spec * material.specular` |
| 最后 `* objectColor` | 取消；颜色进 material 各分量 |
| `shininess = 32` 写死 | `material.shininess` |

---

## 5. GLSL：用 struct 组织 uniform

### 5.1 结构体定义

```glsl
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light {
    vec3 position;   // 点光源位置（世界空间）
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;
```

### 5.2 片元着色器核心

```glsl
void main()
{
    vec3 N = normalize(Normal);
    vec3 L = normalize(light.position - FragPos);
    vec3 V = normalize(viewPos - FragPos);

    // ambient
    vec3 ambient = light.ambient * material.ambient;

    // diffuse
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);

    // specular
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

顶点着色器与 Day02 相同：输出世界空间 `FragPos`、`Normal` 即可。

---

## 6. C++：如何设置 struct uniform？

OpenGL **没有**「一次上传整个 struct」的标准便捷 API，要对**每个成员**设 location。命名规则：

```
material.ambient
material.diffuse
material.specular
material.shininess
light.position
light.ambient
...
```

示例：

```cpp
// 材质：青色塑料感
lightingShader.setVec3("material.ambient",  0.0f, 0.1f, 0.06f);
lightingShader.setVec3("material.diffuse",  0.0f, 0.5f, 0.5f);
lightingShader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);
lightingShader.setFloat("material.shininess", 32.0f);

// 灯光
glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
lightingShader.setVec3("light.position", lightPos.x, lightPos.y, lightPos.z);
lightingShader.setVec3("light.ambient",  lightColor.x * 0.2f, lightColor.y * 0.2f, lightColor.z * 0.2f);
lightingShader.setVec3("light.diffuse",  lightColor.x * 0.5f, lightColor.y * 0.5f, lightColor.z * 0.5f);
lightingShader.setVec3("light.specular", lightColor.x, lightColor.y, lightColor.z);

lightingShader.setVec3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
```

也可在 C++ 侧定义对等结构体，写一个 `setMaterial(shader, mat)` 封装，避免重复敲字符串。

---

## 7. OpenGL / VRML 材质表（官网常用预设）

LearnOpenGL「材质」章引用的经典表来自 **OpenGL teapots.c**（SGI / Mark J. Kilgard, 1994），社区镜像见 [devernay — OpenGL/VRML Materials](http://devernay.free.fr/cours/opengl/materials.html)。  
表中每一行就是一套完整的 `Material`：三个 RGB 向量 + 一个 shininess 系数。

### 7.1 表里每一列是什么意思？

| 列 | 对应字段 | 含义 | 怎么读 |
|----|----------|------|--------|
| Ambient | `material.ambient` | 只受环境光时表面反出的颜色 | 三个数 = RGB，常比 Diffuse 更暗 |
| Diffuse | `material.diffuse` | 漫反射主色（平常说的「物体颜色」） | 受光面主要看这一项 |
| Specular | `material.specular` | 高光斑颜色 / 强度 | 塑料常偏灰白；金属常带金属色 |
| Shininess | `material.shininess` | Phong 高光指数（越大切口越尖） | **见下节：表内值要 ×128** |

对应公式（与第 4 节一致）：

```
ambient  = light.ambient  * material.ambient
diffuse  = light.diffuse  * max(N·L, 0) * material.diffuse
specular = light.specular * pow(max(V·R, 0), material.shininess) * material.specular
```

直觉对照：

| 材质类型 | Ambient / Diffuse | Specular | Shininess（×128 后） |
|----------|-------------------|----------|----------------------|
| 宝石 / 翡翠等 | Diffuse 带宝石色，Ambient 更暗 | 较高、偏亮 | 往往较大（尖高光） |
| 金属（金/铜/银） | Ambient、Diffuse、Specular **都带金属色** | 不偏纯白 | 中等～较大 |
| 塑料 | Ambient 常接近 0；Diffuse 是塑料色 | 灰白中等 | 表内约 0.25 → **32** |
| 橡胶 | Diffuse 有色；Specular 很小或带色但很弱 | 弱 | 表内约 0.078 → **约 10**，糊而暗 |

> **注意**：这是 **Phong 经验参数**，用来「看起来像」某类表面，不是物理测量值；真正的金属 / 宝石要用 PBR 等更复杂模型。

### 7.2 关键警告：Shininess 要乘 128

原表最后一列是 **归一化系数**（约 0～1），固定管线时代写法是：

```
glMaterialf(GL_FRONT, GL_SHININESS, shine * 128.0);
```

因此上传到 **我们的 shader** 时：

```
material.shininess = 表中 Shininess × 128
```

| 表中 Shininess | 实际 `material.shininess` | 观感 |
|----------------|---------------------------|------|
| 0.6 | 76.8 | 翡翠等：高光较尖 |
| 0.4 | 51.2 | 金 / 银常见 |
| 0.25 | 32 | 塑料系列（与教程默认 32 一致） |
| 0.1 | 12.8 | 玉 / 铜等偏软高光 |
| 0.078125 | 10 | 橡胶：高光很糊、很弱 |

Demo / 文档里若直接写 `32.0f`、`76.8f`，指的就是 **已经乘过 128** 的值。

### 7.3 完整材质表

下列 Ambient / Diffuse / Specular 各为 RGB 三个分量；**Shininess 为表中原值**（使用时再 ×128）。

#### 宝石 / 矿石

| Name | Ambient (R,G,B) | Diffuse (R,G,B) | Specular (R,G,B) | Shininess |
|------|-----------------|-----------------|------------------|-----------|
| emerald | 0.0215, 0.1745, 0.0215 | 0.07568, 0.61424, 0.07568 | 0.633, 0.727811, 0.633 | 0.6 |
| jade | 0.135, 0.2225, 0.1575 | 0.54, 0.89, 0.63 | 0.316228, 0.316228, 0.316228 | 0.1 |
| obsidian | 0.05375, 0.05, 0.06625 | 0.18275, 0.17, 0.22525 | 0.332741, 0.328634, 0.346435 | 0.3 |
| pearl | 0.25, 0.20725, 0.20725 | 1.0, 0.829, 0.829 | 0.296648, 0.296648, 0.296648 | 0.088 |
| ruby | 0.1745, 0.01175, 0.01175 | 0.61424, 0.04136, 0.04136 | 0.727811, 0.626959, 0.626959 | 0.6 |
| turquoise | 0.1, 0.18725, 0.1745 | 0.396, 0.74151, 0.69102 | 0.297254, 0.30829, 0.306678 | 0.1 |

#### 金属

| Name | Ambient (R,G,B) | Diffuse (R,G,B) | Specular (R,G,B) | Shininess |
|------|-----------------|-----------------|------------------|-----------|
| brass | 0.329412, 0.223529, 0.027451 | 0.780392, 0.568627, 0.113725 | 0.992157, 0.941176, 0.807843 | 0.21794872 |
| bronze | 0.2125, 0.1275, 0.054 | 0.714, 0.4284, 0.18144 | 0.393548, 0.271906, 0.166721 | 0.2 |
| chrome | 0.25, 0.25, 0.25 | 0.4, 0.4, 0.4 | 0.774597, 0.774597, 0.774597 | 0.6 |
| copper | 0.19125, 0.0735, 0.0225 | 0.7038, 0.27048, 0.0828 | 0.256777, 0.137622, 0.086014 | 0.1 |
| gold | 0.24725, 0.1995, 0.0745 | 0.75164, 0.60648, 0.22648 | 0.628281, 0.555802, 0.366065 | 0.4 |
| silver | 0.19225, 0.19225, 0.19225 | 0.50754, 0.50754, 0.50754 | 0.508273, 0.508273, 0.508273 | 0.4 |

#### 塑料

| Name | Ambient (R,G,B) | Diffuse (R,G,B) | Specular (R,G,B) | Shininess |
|------|-----------------|-----------------|------------------|-----------|
| black plastic | 0.0, 0.0, 0.0 | 0.01, 0.01, 0.01 | 0.50, 0.50, 0.50 | 0.25 |
| cyan plastic | 0.0, 0.1, 0.06 | 0.0, 0.50980392, 0.50980392 | 0.50196078, 0.50196078, 0.50196078 | 0.25 |
| green plastic | 0.0, 0.0, 0.0 | 0.1, 0.35, 0.1 | 0.45, 0.55, 0.45 | 0.25 |
| red plastic | 0.0, 0.0, 0.0 | 0.5, 0.0, 0.0 | 0.7, 0.6, 0.6 | 0.25 |
| white plastic | 0.0, 0.0, 0.0 | 0.55, 0.55, 0.55 | 0.70, 0.70, 0.70 | 0.25 |
| yellow plastic | 0.0, 0.0, 0.0 | 0.5, 0.5, 0.0 | 0.60, 0.60, 0.50 | 0.25 |

#### 橡胶

| Name | Ambient (R,G,B) | Diffuse (R,G,B) | Specular (R,G,B) | Shininess |
|------|-----------------|-----------------|------------------|-----------|
| black rubber | 0.02, 0.02, 0.02 | 0.01, 0.01, 0.01 | 0.4, 0.4, 0.4 | 0.078125 |
| cyan rubber | 0.0, 0.05, 0.05 | 0.4, 0.5, 0.5 | 0.04, 0.7, 0.7 | 0.078125 |
| green rubber | 0.0, 0.05, 0.0 | 0.4, 0.5, 0.4 | 0.04, 0.7, 0.04 | 0.078125 |
| red rubber | 0.05, 0.0, 0.0 | 0.5, 0.4, 0.4 | 0.7, 0.04, 0.04 | 0.078125 |
| white rubber | 0.05, 0.05, 0.05 | 0.5, 0.5, 0.5 | 0.7, 0.7, 0.7 | 0.078125 |
| yellow rubber | 0.05, 0.05, 0.0 | 0.5, 0.5, 0.4 | 0.7, 0.7, 0.04 | 0.078125 |

### 7.4 和灯光怎么配合？（官方练习要点）

表中 ambient **往往不等于** diffuse，且数值按「灯光三分量都是满强度」来设计。LearnOpenGL 提示：

> 要贴近表格观感，把 **灯光强度都设为 `vec3(1.0)`**（`light.ambient / diffuse / specular` 都是白光满强度），再只换材质。

若仍用教程常见的 `ambient=0.2×色、diffuse=0.5×色`，同一套材质会偏暗——这是 **光强缩放**，不是表抄错。当前 Demo 若灯光偏弱，可临时改成三项都 `1.0` 来对照表格。

### 7.5 后续可以怎么用？

| 阶段 | 用法 |
|------|------|
| **现在（Day03）** | 把任意一行抄进 `MaterialPreset`（注意 shininess×128），按键切换；对比塑料 / 金属 / 橡胶差异 |
| **调参实验** | 只改一列（例如只改 Specular），观察高光；固定材质改灯光颜色，看金属色如何「吃」灯色 |
| **多物体场景** | 同一 shader，每物体 draw 前换一套 `setMaterial`；灯参数可共享 |
| **资源化** | 做成数组 / JSON / 表驱动配置，美术改数不必改 shader |
| **下一章光照贴图** | `diffuse` / `specular` 从「常数 RGB」换成 **纹理采样**；`shininess` 仍可保留或进贴图通道——公式不变，只是数据来源变了 |
| **更后面** | Phong 预设 → Blinn-Phong → PBR（金属度/粗糙度）。本表是学习与快速原型的「经验库」，不是最终工业标准 |

C++ 侧最小用法示例（青色塑料，已 ×128）：

```cpp
// cyan plastic：Shininess 0.25 → 32
lightingShader.setVec3("material.ambient",  0.0f, 0.1f, 0.06f);
lightingShader.setVec3("material.diffuse",  0.0f, 0.50980392f, 0.50980392f);
lightingShader.setVec3("material.specular", 0.50196078f, 0.50196078f, 0.50196078f);
lightingShader.setFloat("material.shininess", 0.25f * 128.0f); // = 32
```

### 7.6 调参口诀（表不够用时自己改）

| 想要的效果 | 怎么调 |
|------------|--------|
| 更「塑料亮」 | 增大 `specular`，`shininess` 中等（32~64） |
| 更「哑光」 | 减小 `specular`，降低 `shininess` |
| 阴影里颜色不对 | 调 `ambient`，或 `light.ambient` |
| 主色不对 | 调 `material.diffuse` |
| 高光颜色怪 | 调 `material.specular`（塑料常用灰白；金属跟金属色） |
| 和表对不上 | 检查是否 ×128；灯光是否先用 `vec3(1.0)` 对照 |

---

## 8. 数据流与流程

```
C++ 每帧:
  设置 material.* （可每帧换材质，或启动时设一次）
  设置 light.* 、viewPos、MVP
  Draw 物体

顶点着色器:
  FragPos, Normal → 片元

片元着色器:
  ambient  = light.ambient  * material.ambient
  diffuse  = light.diffuse  * diff * material.diffuse
  specular = light.specular * spec * material.specular
  color    = ambient + diffuse + specular
```

与 Day02 流程相同，只是 **uniform 从「两个颜色」变成「两套 struct」**。

---

## 9. 和当前项目代码的关系

当前工程已实现 **材质 Demo**（`main.cpp` + `lightTest.fs`）：

| 操作 | 材质 |
|------|------|
| 按键 `1` | 青色塑料 Cyan Plastic |
| 按键 `2` | 金色金属感 Gold Metal |
| 按键 `3` | 铜色金属感 Copper Metal |
| 按键 `4` | 黑色橡胶 Black Rubber |

- FS 使用 `Material` / `Light` struct，公式见第 4、5 节；
- C++ 用 `setMaterial` / `setLight` 上传；灯仍绕物体旋转便于对比高光。

控制台会打印当前材质名。对比时注意：塑料高光偏白且范围中等；金属高光更尖、颜色跟金属色；橡胶几乎无亮斑。

---

## 10. 光照贴图预告（下一章在做什么）

材质里的 `diffuse` / `specular` 若改成 **纹理采样**：

```glsl
vec3 diffuseColor  = texture(material.diffuse, TexCoords).rgb;
vec3 specularColor = texture(material.specular, TexCoords).rgb;
```

同一套 Phong 公式就能让箱子各处颜色不同、高光强度随贴图变化——详见 **[Day04 光照贴图](../Day04/光照贴图LightingMaps.md)**。

---

## 11. 小练习

1. 固定灯光，只改 `material.shininess`（8 / 32 / 128），观察高光大小。
2. 把 `material.specular` 设为 `(0,0,0)`，高光应消失，只剩环境+漫反射。
3. `material.diffuse` 与 `material.ambient` 设成不同色，看阴影区与受光区色差。
4. 同一几何体切换两套材质数据（如「橡胶」↔「塑料」），不改 shader 源码。
5. （思考）为何塑料高光常用灰白 `specular`，而不是和 diffuse 同一个颜色？

---

## 12. 公式速查

```
N = normalize(Normal)
L = normalize(light.position - FragPos)
V = normalize(viewPos - FragPos)
R = reflect(-L, N)

ambient  = light.ambient  * material.ambient
diffuse  = light.diffuse  * max(N·L, 0) * material.diffuse
specular = light.specular * pow(max(V·R, 0), material.shininess) * material.specular

FragColor = vec4(ambient + diffuse + specular, 1.0)
```

---

## 13. 阅读顺序与下一步

```
Day01 颜色与场景
  ↓
Day02 基础光照与法线（Phong 三项 + 法线）
  ↓
Day03 材质（本文）← Material / Light 拆分
  ↓
Day04 光照贴图 ← Diffuse / Specular Map
  ↓
投光物 / 多光源
```

---

## 14. 参考文献

1. LearnOpenGL CN — [材质](https://learnopengl-cn.github.io/02%20Lighting/03%20Materials/)
2. LearnOpenGL EN — [Materials](https://learnopengl.com/Lighting/Materials)（文中引用材质表与 cyan plastic 练习）
3. LearnOpenGL CN — [基础光照](https://learnopengl-cn.github.io/02%20Lighting/02%20Basic%20Lighting/)
4. [OpenGL/VRML Materials](http://devernay.free.fr/cours/opengl/materials.html) — teapots.c 材质表（本节第 7 节全文）
5. 《Part2 Day02 — 基础光照与法线》— Phong 公式与当前 Demo
