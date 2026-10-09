# Gamma 校正 Gamma Correction（Day 02）

对应 LearnOpenGL「[Gamma 校正](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/02%20Gamma%20Correction/)」：颜色和光照一直当成线性空间在算，显示器却按大约 **2.2 次幂**把中间亮度压暗。不校正时，中间调偏暗、物理衰减公式也对不上；校正之后才能在线性空间里算光，最后再交给显示器。

| 文档 | 内容 |
|------|------|
| [Day01 高级光照](../Day01/高级光照AdvancedLighting.md) | Blinn-Phong、半程向量 |
| [Part2 Day05 投光物](../../Part2-Lighting/Day05/投光物LightCasters.md) | 点光衰减 `1/(Kc+Kl·d+Kq·d²)` |
| **本文档** | 显示器 Gamma、输出校正、sRGB 贴图、衰减公式为何「看起来不对」 |

**工程对照**：当前 `main.cpp` 仍是地板 + 点光，改成一排四盏灯，方便看衰减半径。`空格` 或 `2` 打开 Gamma（sRGB 贴图 + `1/d²` + 输出 `pow(1/2.2)`）；`1` 关掉（普通 RGB 贴图 + `1/d`，和 Part2 观感接近）。

**前置**：Day01 的 Blinn-Phong；点光距离衰减；Day05 帧缓冲「最后一步再处理颜色」。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Gamma | **灰度系数** | 显示器：输出亮度 ≈ 电压^γ，CRT 大约 γ=2.2 |
| Linear space | **线性空间** | 0.5 的光就是 1.0 的一半光子；公式按物理写 |
| sRGB | **sRGB 颜色空间** | 大约按 γ=2.2 编码，显示器和网图的默认 |
| Gamma correction | **Gamma 校正** | 写出屏幕前先做 `color^(1/γ)`，抵消显示器的压暗 |
| `GL_FRAMEBUFFER_SRGB` | **sRGB 帧缓冲** | 硬件在写入颜色缓冲前自动做这次校正 |
| `GL_SRGB` | **sRGB 纹理内部格式** | 采样时自动把贴图像素解回线性 |
| Attenuation | **衰减** | 光强随距离下降；线性空间里该用平方反比 |

---

## 1. 显示器为什么不是线性的

老式 CRT：电压加倍，亮度大约变成 2.2 次幂，不是两倍。人眼对暗部更敏感，这条曲线和「看起来均匀的灰阶」很接近，所以显示器一直沿用。sRGB 就是按大约 2.2 编码的颜色空间。

物理亮度（光子数量）的灰阶是底下那条：0.5 真的是 1.0 的一半光。人眼 + 显示器看到的是上面那条：中间被抬亮了，暗部层次更多。

问题出在配置颜色的方式：你在屏幕上调「半暗的红」，调的是**已经过显示器曲线**的观感，不是线性值。把这个 0.5 在着色器里 ×2，线性空间应得 1.0；显示器再做一次 ^2.2，中间那段实际亮了四倍以上。以前把灯调得更亮来「看着对」，公式本身是歪的。光照模型越接近物理（衰减、PBR），歪得越明显：暗部糊成一块，颜色叠在一起发脏。

---

## 2. 校正：最后一步把中间调抬亮

思路：写出显示器之前，先乘显示器 Gamma 的**倒数**。

```
校正后 = 线性色 ^ (1/2.2)
显示器再做 ^2.2 → 你看见的 ≈ 线性色
```

中间亮度被抬起来，显示器再压回去，两端的黑白不变。

两条路：

**A. 硬件 sRGB 帧缓冲**（省事，控制少）

```cpp
glEnable(GL_FRAMEBUFFER_SRGB);
```

之后写入颜色缓冲（含默认窗口）时自动按 sRGB 编码。中间 FBO 若还要拿去算光，应保持线性，只在**最后那个**要上屏的缓冲开 sRGB。

**B. 片元着色器里自己 `pow`**（本 Demo）

```glsl
float gamma = 2.2;
fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / gamma));
```

每个会画到屏幕的着色器都得写；更干净的做法是后处理全屏四边形只做一次。本 Demo 只有地板这一个最终颜色输出，直接写在 `gamma.fs` 末尾，并用 `uniform bool gamma` 开关。

无论 A 还是 B：只在**上屏前做一次**。中间结果先 Gamma 再拿去模糊、混合、再照一次，等于在错误的空间里算。

---

## 3. sRGB 贴图：别校正两次

贴图是在显示器上画的，像素已经按 sRGB 编好。以前不校正：贴图 sRGB、输出也当 sRGB，歪打正着。现在着色器按线性算、最后再 `pow(1/2.2)`，若把贴图字节当线性 RGB 来乘，等于 **Gamma 了两次**，漫反射会过亮、发白。

采样前解回线性：

```glsl
vec3 diffuseColor = pow(texture(diffuse, uv).rgb, vec3(2.2));
```

每张 albedo 都写一遍很烦。创建纹理时把内部格式改成 `GL_SRGB` / `GL_SRGB_ALPHA`，`texture()` 时驱动自动解码：

```cpp
glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
```

**只有「在显示器上画出来的颜色图」才是 sRGB**：漫反射 / albedo。高光贴图、法线、粗糙度、金属度是数据，本来就是线性的，标成 `GL_SRGB` 光照会坏。

本 Demo 同一张 `container.jpg` 加载两份：一份 `GL_RGB`，一份 `GL_SRGB`。关掉 Gamma 绑前者（和旧 Demo 一样）；打开 Gamma 绑后者，再在着色器末尾 `pow(1/2.2)`，整条链只校正一次。

---

## 4. 衰减：为什么以前不敢用 `1/d²`

物理上点光强度按距离平方下降：

```
attenuation = 1.0 / (distance * distance)
```

不校正时这圈光缩得很死，所以 Part2 用了 `1/d` 或 `1/(Kc+Kl·d+Kq·d²)` 去「看着像」。

原因：衰减改的是线性亮度，显示器再 ^2.2。未校正时你看见的其实接近 `(1/d²)^2.2`，衰减被再乘一次指数，当然过狠。`1/d` 经过显示器之后有点像 `1/d²`，才显得自然。

打开 Gamma 之后，平方反比突然合理；`1/d` 反而太飘、照太远。本 Demo 跟官网一样：**关 Gamma 用 `1/d`，开 Gamma 用 `1/d²`**，灯的颜色和贴图资源不变，对比的是整条颜色管线。

Part2 那套 `Kc/Kl/Kq` 在校正后仍然能用，只是参数要重调，因为它给了半径控制，不是单纯 `1/d²`。

---

## 5. 和本工程怎么接

### 5.1 画面与按键

地板仍是 Day01 那块平面。四盏点光排成一排（暗 → 亮），小立方体标位置。退后看光圈半径：关校正时四盏灯都铺得比较开；打开后近处更亮、远处掉得更快，暗部细节也更清楚。

| 键 | 作用 |
|----|------|
| `1`（默认） | 无校正：`GL_RGB` 贴图 + `1/d` + 直接输出 |
| `2` 或 `空格` | 有校正：`GL_SRGB` 贴图 + `1/d²` + `pow(rgb, 1/2.2)` |
| WASD、鼠标、滚轮、Esc | 与前面几章相同 |

### 5.2 着色器（对照 `shaders/gamma.*`）

顶点着色器与 Day01 地板相同（世界坐标、法线、UV）。

片元：四盏灯做 Blinn-Phong，贡献相加；衰减按 `gamma` 二选一；最后若开启则做输出校正。灯立方体仍走 `lamp.vs` / `lamp.fs`（纯色标位置，不参与这条对比）。

### 5.3 每一帧

按开关选 `floorLinear` 或 `floorSRGB`，上传 `lightPositions[4]`、`lightColors[4]`、`gamma`。

相关文件：`src/cppfile/main.cpp`，`shaders/gamma.vs` / `gamma.fs`。

---

## 6. 容易踩的坑

| 现象 | 原因 |
|------|------|
| 打开后地板爆亮、发白 | albedo 仍按 `GL_RGB` 采样，又做了一次输出 `pow`。应换 `GL_SRGB` |
| 法线 / 高光贴图发怪 | 数据贴图被标成了 `GL_SRGB`。只给漫反射用 sRGB |
| 中间 FBO 颜色发灰或发爆 | 后处理前就校正了。离屏缓冲保持线性，只在上屏那一趟校正 |
| 开了 `GL_FRAMEBUFFER_SRGB` 着色器里又 `pow` | 校正两次 |
| 开校正后灯几乎只剩脚下一小圈 | 正常：`1/d²` 掉得快。把灯抬高一点或把灯色加亮（本 Demo 已按官网排了四盏不同亮度） |
| 关校正时用 `1/d²` 对比 | 和官网不一致，关着会看起来「衰减过狠」，那是显示器曲线造成的假象 |

---

## 7. 小练习

1. 屏幕上把线性 0.5 的红 ×2，不校正时显示器上为什么不是「亮一倍」？
2. 为什么校正必须放在**最后**一次写入要显示的颜色？中间 FBO 该不该 `pow`？
3. `container.jpg` 为什么要 `GL_SRGB`，法线贴图为什么不要？
4. 关 Gamma 时 `1/d` 看着自然，开 Gamma 后为什么改成 `1/d²`？
5. （思考）`glEnable(GL_FRAMEBUFFER_SRGB)` 和着色器里 `pow(1/2.2)` 各适合什么管线？多 Pass 时选哪个更不容易忘？

---

## 8. 阅读顺序与下一步

```
Part5 Day01 Blinn-Phong
  ↓
Part5 Day02 Gamma 校正（本文）← 线性空间里算光，上屏前再编码
  ↓
阴影映射 …
```

---

## 9. 参考文献

1. LearnOpenGL CN — [Gamma 校正](https://learnopengl-cn.github.io/05%20Advanced%20Lighting/02%20Gamma%20Correction/)
2. LearnOpenGL EN — [Gamma Correction](https://learnopengl.com/Advanced-Lighting/Gamma-Correction)
3. 《Part2 Day05 — 投光物》— `Kc/Kl/Kq` 衰减；校正后参数要重调
4. 本工程 `src/cppfile/main.cpp` — 四盏灯切换 sRGB 贴图 / 平方衰减 / 输出 `pow`
