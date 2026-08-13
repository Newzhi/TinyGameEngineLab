cmake_minimum_required(VERSION 3.17)
project(learn_opengl_proj LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出 compile_commands.json，供编辑器的 clangd 解析头文件路径，
# 否则 IDE 会误报“找不到 iostream / glm / assimp”之类的假错误。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 第三方依赖根目录（相对 Proj/ → ../Depends），不联网拉取
set(DEPENDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Depends")

# ---------------------------------------------------------------------------
# 预编译依赖（GLFW / Assimp）
#   这些库已用当前 MinGW 工具链提前编译好，安装在 Depends/prebuilt/。
#   头文件在 prebuilt/include，静态库(.a) 在 prebuilt/lib。
#   好处：本工程不再 add_subdirectory 第三方源码，Reload CMake 和构建都快很多。
#   如需重新生成这些库，见 Depends/README.md（用 cmake --install 到该前缀）。
# ---------------------------------------------------------------------------
set(PREBUILT_DIR "${DEPENDS_DIR}/prebuilt")

# ---------- GLAD（仍由本工程编译，只有一个 glad.c）----------
add_library(glad STATIC "${DEPENDS_DIR}/glad/src/glad.c")
target_include_directories(glad PUBLIC "${DEPENDS_DIR}/glad/include")

# ---------- 可执行目标 ----------
add_executable(main
    src/cppfile/main.cpp
    src/cppfile/stb_image_impl.cpp
    src/headfile/Shader.h
    src/headfile/Camera.h
    src/headfile/Mesh.h
    src/headfile/Model.h
)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/headfile
    ${PREBUILT_DIR}/include   # assimp/、GLFW/（预编译库的头）
    ${DEPENDS_DIR}            # glm: #include <glm/...>（header-only）
    ${DEPENDS_DIR}/stb        # #include "stb_image.h"
)

# 预编译静态库所在目录（供链接器查找 .a）
target_link_directories(main PRIVATE ${PREBUILT_DIR}/lib)

# 链接说明：
#   glfw3      窗口/输入（预编译）
#   assimp     模型导入（预编译）
#   zlibstatic assimp 的压缩依赖，静态链接时必须显式带上
#   opengl32   Windows 的 OpenGL
#   gdi32      GLFW 在 Windows 上的图形依赖
if (WIN32)
    target_link_libraries(main PRIVATE glad glfw3 assimp zlibstatic opengl32 gdi32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(main PRIVATE glad glfw3 assimp z OpenGL::GL)
endif()

# 着色器每次构建后同步到运行目录，改 .vs/.fs 只需 Build，不必 Reload CMake
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            ${CMAKE_CURRENT_BINARY_DIR}/shaders
    COMMENT "Syncing shaders to build directory")

# 模型/贴图同样每次构建同步：configure 期的 file(COPY) 只跑一次，
# 之后新增的贴图不会进构建目录，运行时 stbi_load 会静默失败。
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Resource
            ${CMAKE_CURRENT_BINARY_DIR}/Resource
    COMMENT "Syncing resources to build directory")
