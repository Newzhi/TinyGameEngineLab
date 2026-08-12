cmake_minimum_required(VERSION 3.17)
project(learn_opengl_proj LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 第三方依赖根目录（相对 Proj/ → ../Depends），不联网拉取
set(DEPENDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Depends")

# ---------- GLFW（本地源码）----------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPENDS_DIR}/glfw" "${CMAKE_BINARY_DIR}/_deps/glfw")

# ---------- Assimp（本地源码，静态链接，少管 DLL）----------
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
# 与主工程一起编时用静态库，避免运行目录再拷 dll
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPENDS_DIR}/assimp-5.4.3" "${CMAKE_BINARY_DIR}/_deps/assimp")

# ---------- GLAD ----------
add_library(glad STATIC "${DEPENDS_DIR}/glad/src/glad.c"
        src/headfile/Mesh.h)
target_include_directories(glad PUBLIC "${DEPENDS_DIR}/glad/include")

add_executable(main
    src/cppfile/main.cpp
    src/cppfile/stb_image_impl.cpp
    src/headfile/Shader.h
    src/headfile/Camera.h
)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/headfile
    ${DEPENDS_DIR}          # glm: #include <glm/...>
    ${DEPENDS_DIR}/stb      # #include "stb_image.h"
)

# 着色器与资源复制到构建目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Resource DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if (WIN32)
    target_link_libraries(main PRIVATE glad glfw assimp opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(main PRIVATE glad glfw assimp OpenGL::GL)
endif()
