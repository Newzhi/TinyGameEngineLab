# 纹理资源目录

把 LearnOpenGL 教程用到的图片放在这里，例如：

- `container.jpg` / `container2.png` / `container2_specular.png`
- `grass.png`、`blending_transparent_window.png`（Part4 Day03 混合）

程序运行时通过相对路径加载，例如：

```cpp
stbi_load("Resource/Texture/container.jpg", ...);
```

CMake 在**每次构建后**（POST_BUILD）把整个 `Resource/` 同步到构建输出目录，新增图片直接 Build 即可生效。
