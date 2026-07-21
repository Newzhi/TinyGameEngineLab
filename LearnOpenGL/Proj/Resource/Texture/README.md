# 纹理资源目录

把 LearnOpenGL 教程用到的图片放在这里，例如：

- `container.jpg`
- `awesomeface.png`

程序运行时通过相对路径加载，例如：

```cpp
stbi_load("Resource/Texture/container.jpg", ...);
```

CMake 会在配置时将整个 `Resource/` 目录复制到构建输出目录，无需手动拷贝。
