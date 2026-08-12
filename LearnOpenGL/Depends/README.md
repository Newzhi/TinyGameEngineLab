# Third-party dependencies (offline / vendored)

All external libraries live under this directory. The main project (`Proj/`)
must **not** use CMake FetchContent / online downloads for these deps.

## Layout

| Directory | Library | How it is used |
|-----------|---------|----------------|
| `glad/` | GLAD OpenGL loader | Static lib target `glad` |
| `glfw/` | GLFW 3.x window/input | `add_subdirectory` → link `glfw` |
| `glm/` | GLM math (header-only) | include `Depends/` then `#include <glm/...>` |
| `stb/` | stb_image etc. | include `Depends/stb` |
| `assimp-5.4.3/` | Assimp model import | `add_subdirectory` → link `assimp` |

## Rules

1. Put new third-party code here only (not under `Proj/includes` or `Proj/src`).
2. Prefer source + `add_subdirectory` over prebuilt `.lib/.dll` when the lib has CMake.
3. Record version / license when upgrading a dependency.
4. Do **not** store toolchain installs here (CMake, compilers). Those belong in `Tools/` or the system.

## Versions (current)

- glad: generated loader (see `glad/` contents)
- glfw: 3.4 (vendored from previous FetchContent source)
- glm: as previously under `Proj/includes/glm`
- stb: stb_image (single header)
- assimp: 5.4.3
