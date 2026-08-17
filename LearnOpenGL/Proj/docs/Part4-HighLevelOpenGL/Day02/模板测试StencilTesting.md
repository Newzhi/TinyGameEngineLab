# 模板测试 Stencil Testing（Day 02）

对应 LearnOpenGL「[模板测试](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/02%20Stencil%20testing/)」：在深度测试之外，再用一块 **按像素存整数标签** 的缓冲，决定片段留不留、以及如何改写这些标签。

| 文档 | 内容 |
|------|------|
| [Day01 深度测试](../Day01/深度测试DepthTesting.md) | 按「远近」丢弃片段 |
| **本文档** | 模板缓冲、`glStencilFunc` / `glStencilOp` / mask、通过/失败执行流程、物体轮廓与其它用法思路 |
| [Day03 混合](../Day03/混合Blending.md) | alpha、`discard`、半透明排序 |

**工程现状**：本工程目前**尚未接入**模板测试 API；下文以官网步骤 + 可粘贴示意为准，便于你对照实现。

**前置**：已理解深度测试、每帧 `glClear`、片段在 FS 之后还可能被丢弃。

---

## 0. 术语中英对照

| 英文 | 中文 | 一句话 |
|------|------|--------|
| Stencil buffer | **模板缓冲** | 每个像素存一个整数「标签」（常见 8 位，0～255） |
| Stencil testing | **模板测试** | 用标签与参考值比较，决定丢弃/保留片段 |
| Stencil value | **模板值** | 某像素当前存的那个整数 |
| Reference value (`ref`) | **参考值** | `glStencilFunc` 里拿来比较的数 |
| Stencil mask | **模板掩码** | 写入或比较前做按位 AND 的位掩码 |
| `glStencilFunc` | **模板函数** | 怎么比：等于？不等于？永远过？ |
| `glStencilOp` | **模板操作** | 测过/测挂时，缓冲里的值怎么改 |
| Object outlining | **物体轮廓** | 用模板「打孔」再画放大版，得到描边 |

---

## 1. 模板测试是干什么的？

深度测试问的是：**这个像素上谁更近？**  
模板测试问的是：**这个像素的「标签」满不满足我设的规则？**

官网顺序（逻辑上）：

```
片段着色器跑完
        │
        ▼
  模板测试 ──失败→ 丢弃
        │通过
        ▼
  深度测试 ──失败→ 丢弃（默认时常不改模板，见 glStencilOp）
        │通过
        ▼
  写入颜色（以及按规则更新深度 / 模板）
```

模板缓冲通常是 **8 位整数/像素** → 每像素最多 256 种标签。  
GLFW 一般会自动带上模板缓冲；换别的窗口库时要确认是否创建了 stencil。

### 1.1 一个直觉例子

```
1. 整屏模板先清成 0
2. 画一个「遮罩形状」，把该区域模板写成 1（只打标签，或边画边打）
3. 再画真正的内容，但规定：只有模板 == 1 的像素才通过
→ 内容只出现在遮罩形状里（像剪纸窗）
```

---

## 2. 和深度测试怎么配合记？

| | 深度缓冲 | 模板缓冲 |
|--|----------|----------|
| 存什么 | 远近（常为 float / 定点数深度） | 整数标签 |
| 默认用途 | 遮挡正确 | 自定义遮罩 / 特效 |
| 开启 | `glEnable(GL_DEPTH_TEST)` | `glEnable(GL_STENCIL_TEST)` |
| 每帧清除 | `GL_DEPTH_BUFFER_BIT` | `GL_STENCIL_BUFFER_BIT` |
| 禁止写入 | `glDepthMask(GL_FALSE)` | `glStencilMask(0x00)` |

通用两阶段套路（官网总结）：

1. **写模板**：打开写入，画物体 A，把像素打上标签  
2. **读模板**：关掉写入（或换规则），画物体 B，按标签决定画不画  

```
启用模板写入
  渲染物体 → 更新模板缓冲
禁用模板写入
  再渲染（其它）物体 → 根据模板值丢弃特定片段
```

---

## 3. 最小启用与清除

```cpp
glEnable(GL_STENCIL_TEST);

// 每帧：颜色 + 深度 + 模板 一起清
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
```

清模板时默认清成 0（也可用 `glClearStencil` 改清除值）。

---

## 4. 函数说明（核心 API）

### 4.1 `glStencilMask` —— 写不写得进缓冲？

```cpp
glStencilMask(0xFF); // 各位可写（通常表示「允许写入」）
glStencilMask(0x00); // 与运算后全变 0 → 等效禁止写入
```

类比 `glDepthMask`：  
- `0xFF` ≈ 深度可写  
- `0x00` ≈ 深度只读  

高级用法里可以用自定义位掩码只动某几位；入门阶段几乎只用这两个。

### 4.2 `glStencilFunc(func, ref, mask)` —— 怎么比？

比较前会做：

```
(储存的模板值 & mask)  ??  (ref & mask)
```

`??` 由 `func` 决定。

| `func` | 含义（通过条件） |
|--------|------------------|
| `GL_ALWAYS` | 永远通过（常用来「刷标签」） |
| `GL_NEVER` | 永远不通过 |
| `GL_EQUAL` | 等于 `ref` |
| `GL_NOTEQUAL` | 不等于 `ref` |
| `GL_LESS` / `GL_LEQUAL` | 小于 / 小于等于 |
| `GL_GREATER` / `GL_GEQUAL` | 大于 / 大于等于 |

官网简单遮罩例子：

```cpp
glStencilFunc(GL_EQUAL, 1, 0xFF);
// 模板值 == 1 才通过并绘制，否则丢弃
```

注意：`glStencilFunc` **只决定测过不过**，**不单独决定**缓冲如何被改写——改写看 `glStencilOp`。

### 4.3 `glStencilOp(sfail, dpfail, dppass)` —— 测完怎么改标签？

三个参数对应三种结果：

| 参数 | 何时触发 |
|------|----------|
| `sfail` | **模板测试失败** |
| `dpfail` | 模板通过，但 **深度测试失败** |
| `dppass` | 模板 **和** 深度 **都通过** |

每个参数可选行为：

| 行为 | 描述 |
|------|------|
| `GL_KEEP` | 保持原值（默认三参数全是 KEEP → **从不改模板**） |
| `GL_ZERO` | 写成 0 |
| `GL_REPLACE` | 写成 `glStencilFunc` 的 `ref` |
| `GL_INCR` / `GL_DECR` | 加 1 / 减 1（到顶/到底则停） |
| `GL_INCR_WRAP` / `GL_DECR_WRAP` | 加减，溢出则回绕 |
| `GL_INVERT` | 按位取反 |

默认 `(GL_KEEP, GL_KEEP, GL_KEEP)`：测来测去标签不变。  
**想写入模板，至少改其中一个参数。**

轮廓常用组合：

```cpp
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
// 模板失败：不动
// 深度失败：不动
// 两者都过：把该像素模板写成 ref（例如 1）
```

### 4.4 三者怎么配合记？

```
glStencilFunc  →  用什么规则判定「过/不过」
glStencilOp    →  过/不过之后，标签怎么变
glStencilMask  →  变的时候，哪些位真的写得进去
```

### 4.5 通过 / 失败：真正的执行流程（重要）

很多人会以为：模板「成功」就调用某个函数、「失败」就调用另一个函数。  
**不是这样。** OpenGL **没有**类似下面的回调：

```cpp
if (stencilPass)
    onStencilSuccess();  // ← 不存在
else
    onStencilFail();
```

你调用的是 **设规则**；**每个像素**在 `glDraw*` 时由 GPU **自动判断**。

| 你写的 | 实际在干什么 |
|--------|----------------|
| `glStencilFunc(...)` | 告诉 GPU：**怎么判断**通过还是失败 |
| `glStencilOp(...)` | 告诉 GPU：通过/失败后，**模板缓冲里的数字怎么改** |
| `glDrawArrays(...)` / `Draw` | 真正开始画；每个片段按规则自动测一遍 |

#### 单个像素上发生了什么？

例如设置了 `glStencilFunc(GL_EQUAL, 1, 0xFF)` 再画全屏橙色：

```
取出该像素模板缓冲里的值（中央可能是 1，外面是 0）
        │
        ▼
和 ref=1 比较（GL_EQUAL）
        │
   ┌────┴────┐
通过(==1)   失败(!=1)
   │           │
   ▼           ▼
把颜色写入    直接丢掉这个片段
颜色缓冲      （这一遍这里不上色）
```

所以：

- **通过** = 这一次 Draw 里，该像素允许写入颜色（等）  
- **失败** = 该像素片段被丢弃，相当于「这里不画」  

失败时 **不会自动变成另一种颜色**。

#### 「通过画画面 A、失败画画面 B」为什么要画两遍？

因为失败只是「丢掉」，不是「换成 B」。要两种画面，就用 **两次 Draw + 两套 Func**：

```
第 1 步：盖章
  Func = ALWAYS，Op = REPLACE，Mask 可写
  画中央矩形（常关颜色写入）
  → 中央模板 = 1，周围仍是 0

第 2 步：画面 B（「不通过」区域想看到的内容）
  Func = NOTEQUAL, 1
  画蓝色全屏
  → 只有模板 != 1 的像素通过 → 周围变蓝
  → 中央模板是 1，本遍失败 → 中央不上色

第 3 步：画面 A（「通过」区域想看到的内容）
  Func = EQUAL, 1
  画橙色全屏
  → 只有模板 == 1 的像素通过 → 中央变橙
  → 周围本遍失败 → 保留上一步的蓝
```

一句话：

> **通过/失败决定的是「这一次 Draw 里哪些像素能留下」；**  
> **另一种画面是另一次 Draw、换一套 `glStencilFunc` 画上去的。**

`glStencilOp` 同样不是回调：三个参数只表示「模板挂 / 深度挂 / 都过」时 **标签怎么改**（KEEP、REPLACE…），改的是模板缓冲里的整数，不是去调你的 C++ 函数。

本工程 `main.cpp` 的最简 Demo 就是按上面三步写的，可对照阅读。

---

## 5. 经典效果：物体轮廓（Object Outlining）

策略游戏「选中单位描边」就靠这个思路。

### 5.1 思路（先想清楚再记代码）

```
① 正常画物体，同时把物体盖住的像素模板写成 1
② 禁止再写模板；关掉（或弱化）深度干扰
③ 把同一物体稍微放大，用纯色 shader 再画一遍
④ 规定：只有模板 ≠ 1 的像素才画
   → 放大版里「原来物体已经占用」的部分被挖掉
   → 剩下的一圈就是描边
```

像两张纸：先盖章标出物体区域，再画一张稍大的色纸，把盖章区域剪掉，只留边缘。

### 5.2 步骤拆解（对照官网）

1. 启用模板写入  
2. `glStencilFunc(GL_ALWAYS, 1, 0xFF)`：物体片段一律通过  
3. `glStencilOp(..., GL_REPLACE)`：通过则写成 1  
4. 画物体  
5. `glStencilMask(0x00)`：禁止再改模板  
6. `glStencilFunc(GL_NOTEQUAL, 1, 0xFF)`：只画「不是 1」的地方  
7. （常）`glDisable(GL_DEPTH_TEST)`：避免描边被地板挡住  
8. 用单色 shader 画**略放大**的物体  
9. 恢复 `glStencilMask(0xFF)`、重新 `glEnable(GL_DEPTH_TEST)`  

### 5.3 单色边框 shader（示意）

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.04, 0.28, 0.26, 1.0); // 描边色
}
```

放大可以在 CPU 侧 `scale` 稍大于 1，或沿法线挤出（更稳的描边，进阶做法）。

### 5.4 一帧流程伪代码（官网完整步骤）

```cpp
glEnable(GL_DEPTH_TEST);
glEnable(GL_STENCIL_TEST);
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

// --- 地板：不要污染模板 ---
glStencilMask(0x00);
normalShader.use();
DrawFloor();

// --- 箱子：写入模板 = 1 ---
glStencilFunc(GL_ALWAYS, 1, 0xFF);
glStencilMask(0xFF);
DrawTwoContainers();

// --- 放大箱子：只画模板 ≠ 1 的部分 = 描边 ---
glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
glStencilMask(0x00);
glDisable(GL_DEPTH_TEST);
shaderSingleColor.use();
DrawTwoScaledUpContainers();

// --- 恢复状态，别影响后续绘制 ---
glStencilMask(0xFF);
glEnable(GL_DEPTH_TEST);
```

### 5.5 效果细节与坑

| 现象 / 选择 | 说明 |
|-------------|------|
| 多个物体描边粘在一起 | 官网默认如此；策略游戏多选时常常正好需要「合并轮廓」 |
| 要每个物体独立完整描边 | 往往要**逐物体**清模板再描，并仔细处理深度 |
| 关掉深度再画描边 | 描边不会被地面挡住，但可能穿到不该穿的地方，需权衡 |
| 描边太粗/太细 | 调放大系数；过大易穿模，过小几乎看不见 |
| 想更自然 | 可后期高斯模糊描边（后处理方向） |

---

## 6. 其它常见用法与思路（不只轮廓）

模板是「像素级布尔/计数遮罩」，用途很广：

### 6.1 镜子 / 传送门窗口

```
1. 画镜子形状，模板写成 1（可关掉颜色写入，只打洞）
2. 只在模板 == 1 处画「镜子里的世界」
3. 再画镜框
```

后视镜、平面反射剪裁都属这类。

### 6.2 UI / 血条 / 圆形头像裁剪

```
先画圆形到模板，再画头像且 GL_EQUAL
→ 头像只出现在圆里
```

### 6.3 阴影体积（Shadow Volume，进阶）

用模板计数：光源看过去，穿过阴影体积正面 +1、背面 −1，最后模板非 0 的区域在影里。经典但实现重，知道思路即可。

### 6.4 平面反射时「只在地面上」画倒影

```
地面写入模板 → 倒影仅在模板命中处绘制 → 再画不透明地面/物体
```

### 6.5 选中高亮、遮挡轮廓（游戏里常见变体）

- **遮挡处也显示轮廓**：描边 pass 关深度或改深度函数，被墙挡住仍能看见选中描边  
- **仅可见部分描边**：保留深度测试，只描「看得到」的边缘  

思路都是：**第一遍打标签，第二遍按标签裁剪绘制**。

### 6.6 和帧缓冲 / 后处理的关系

复杂描边、外发光常变成：

```
离屏渲染选中物体 ID/mask → 后处理找边缘 → 叠回屏幕
```

模板是「同一次前向里」的轻量方案；后处理更灵活但更重。入门先掌握模板轮廓即可。

---

## 7. 调试 checklist

| 问题 | 排查 |
|------|------|
| 模板完全没效果 | 是否 `glEnable(GL_STENCIL_TEST)`？是否 clear 了 `GL_STENCIL_BUFFER_BIT`？ |
| 标签写不进去 | `glStencilMask` 是否 `0x00`？`glStencilOp` 是否仍全 `KEEP`？ |
| 第二遍什么都没画 | `func/ref` 是否反了（`EQUAL`/`NOTEQUAL`）？mask 是否把位全抹掉？ |
| 描边被地板切掉 | 描边 pass 是否需要临时 `glDisable(GL_DEPTH_TEST)`？ |
| 状态污染下一帧 | 是否恢复 `StencilMask`、`DepthTest`、`StencilFunc`？ |
| GLFW 外窗口库 | 是否真的创建了 stencil 附件？ |

---

## 8. 和本工程怎么接

### 8.1 最简矩形掩码 Demo（仍可参考 `stencilColor.*`）

中央矩形写模板 `1` → `NOTEQUAL` 画蓝 → `EQUAL` 画橙。见此前 §4.5。

### 8.2 物体轮廓 Demo（可切回）

按官网步骤：地板不写模板 → 背包 `ALWAYS+REPLACE` 写 `1` → `NOTEQUAL` 画放大纯色版。相关思路见 §5。

### 8.3 平面镜子 Demo（当前 `main.cpp`）

对应 §6.1，用模板把倒影裁在镜面里：

1. **真实场景**：地板 + 背包，`StencilMask(0x00)`  
2. **打洞**：画镜面四边形，`ALWAYS+REPLACE` 写成 `1`；`ColorMask/DepthMask` 关闭（只打标签）  
3. **倒影**：`EQUAL 1`，用反射矩阵 `T(z)·S(1,1,-1)·T(-z)` 再画一遍场景；反射会翻转绕序，故临时 `CullFace(FRONT)`  
4. **玻璃 + 镜框**：半透明 tint 叠在倒影上，再画不透明边框  

相关文件：

| 文件 | 作用 |
|------|------|
| `src/cppfile/main.cpp` | 镜子四步流程 |
| `shaders/outline.vs` / `outline.fs` | 地板 / 镜面 / 镜框纯色（`uAlpha` 支持玻璃） |
| `shaders/model.vs` / `model.fs` | 背包正常着色 |

运行：`WASD` + 鼠标；墙上一面镜子里应能看到背包与地板的左右翻转倒影。物体都放在镜前，避免镜后几何被错误反射（完整版需 `gl_ClipDistance` 裁剪平面）。

---

## 9. 小练习

1. 用自己的话区分：`glStencilFunc` 和 `glStencilOp` 各管什么？  
2. 为什么说模板「通过/失败」不是回调函数？那是在什么时候、对什么东西做判断的？  
3. 为什么「通过画 A、失败画 B」需要两次 `glDraw*`，而不是一次 Draw 里分支？  
4. 为什么默认 `glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP)` 时，开了模板测试也「写不进标签」？  
5. 轮廓算法里，为什么第二遍要用 `GL_NOTEQUAL` 而不是 `GL_EQUAL`？  
6. 画地板时为什么常先 `glStencilMask(0x00)`？  
7. （思考）若希望两个重叠选中单位**各自**有完整描边而不是粘成一圈，流程要怎么改？  
8. （可选）对照官网仓库 `4.advanced_opengl/2.stencil_testing` 源码，把步骤编号写在旁注里。

---

## 10. 阅读顺序与下一步

```
Day01 深度测试（远近）
  ↓
Day02 模板测试（本文）← 像素标签 + 轮廓
  ↓
Day03 混合 Blending → Day04 面剔除 → 帧缓冲 …
```

---

## 11. 参考文献

1. LearnOpenGL CN — [模板测试](https://learnopengl-cn.github.io/04%20Advanced%20OpenGL/02%20Stencil%20testing/)  
2. LearnOpenGL EN — [Stencil testing](https://learnopengl.com/Advanced-OpenGL/Stencil-testing)  
3. 《Part4 Day01 — 深度测试》— 测试顺序与 mask 的对照  
4. 官网示例：物体轮廓（选中描边）完整流程  
