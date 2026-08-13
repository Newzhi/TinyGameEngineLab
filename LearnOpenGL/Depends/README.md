# Third-party dependencies (offline / vendored)

All external libraries live under this directory. The main project (`Proj/`)
must **not** use CMake FetchContent / online downloads for these deps.

## Layout

| Directory | Library | How it is used |
|-----------|---------|----------------|
| `glad/` | GLAD OpenGL loader | Static lib target `glad`（本工程直接编译 `glad.c`） |
| `glfw/` | GLFW 3.x window/input | 源码；已预编译进 `prebuilt/`，链接 `glfw3` |
| `glm/` | GLM math (header-only) | include `Depends/` then `#include <glm/...>` |
| `stb/` | stb_image etc. | include `Depends/stb` |
| `assimp-5.4.3/` | Assimp model import | 源码；已预编译进 `prebuilt/`，链接 `assimp` + `zlibstatic` |
| `prebuilt/` | **预编译产物** | `include/` 头文件 + `lib/` 静态库(.a)，供主工程直接链接 |

## prebuilt/ 是怎么来的（用当前 MinGW 生成）

`Proj/CMakeLists.txt` 不再 `add_subdirectory` 第三方源码，而是链接 `prebuilt/lib` 里
已经编译好的静态库。这样 Reload CMake 与构建都快很多（不再重复编译 Assimp）。

⚠️ 预编译库必须与使用它的编译器一致。当前用的是 **CLion 自带 MinGW GCC**。
换编译器/大版本后需要重新生成。重新生成命令（在仓库根 `LearnOpenGL/` 下，
先把 CLion 的 mingw `bin` 加入 PATH，`CMAKE` 指向 CLion 自带 cmake）：

```powershell
# Assimp（只编 OBJ/FBX/GLTF，去掉 Debug 后缀，静态）
& $CMAKE -S Depends/assimp-5.4.3 -B Depends/_build/assimp -G "MinGW Makefiles" `
    -DCMAKE_INSTALL_PREFIX=Depends/prebuilt -DBUILD_SHARED_LIBS=OFF `
    -DASSIMP_INSTALL=ON -DASSIMP_NO_EXPORT=ON `
    -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_BUILD_SAMPLES=OFF `
    -DASSIMP_INJECT_DEBUG_POSTFIX=OFF -DCMAKE_DEBUG_POSTFIX= `
    -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF `
    -DASSIMP_BUILD_OBJ_IMPORTER=ON -DASSIMP_BUILD_FBX_IMPORTER=ON -DASSIMP_BUILD_GLTF_IMPORTER=ON
& $CMAKE --build Depends/_build/assimp --target install -j

# GLFW（静态）
& $CMAKE -S Depends/glfw -B Depends/_build/glfw -G "MinGW Makefiles" `
    -DCMAKE_INSTALL_PREFIX=Depends/prebuilt -DBUILD_SHARED_LIBS=OFF `
    -DGLFW_BUILD_DOCS=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_EXAMPLES=OFF
& $CMAKE --build Depends/_build/glfw --target install -j
```

生成后 `Depends/_build/` 只是临时构建目录，可以删除；保留 `Depends/prebuilt/`。

## Rules

1. Put new third-party code here only (not under `Proj/includes` or `Proj/src`).
2. 第三方优先预编译到 `prebuilt/` 再链接；`glad` 例外（单个 `.c`，直接编）。
3. Record version / license when upgrading a dependency.
4. Do **not** store toolchain installs here (CMake, compilers). Those belong in `Tools/` or the system.

## Versions (current)

- glad: generated loader (see `glad/` contents)
- glfw: 3.4 (vendored from previous FetchContent source)
- glm: as previously under `Proj/includes/glm`
- stb: stb_image (single header)
- assimp: 5.4.3
