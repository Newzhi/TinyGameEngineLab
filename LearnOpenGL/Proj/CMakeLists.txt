cmake_minimum_required(VERSION 3.17)
project(learn_opengl_proj LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 仅构建 GLFW 库本体，关闭其文档/示例/测试
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# 克隆后由 CMake 自动拉取 GLFW 3.4，无需本地 Depends/glfw-3.4
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

add_executable(main
    src/glad.c
    src/cppfile/main.cpp
    src/cppfile/stb_image_impl.cpp
    src/headfile/Shader.h
    src/headfile/Camera.h
)

# 头文件搜索路径：
# - includes/：GLAD、GLFW 等第三方库
# - src/headfile/：项目头文件（Shader.h、stb_image.h）
target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}/src/headfile
)

# 把着色器和资源文件复制到构建目录，便于运行时按相对路径读取
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Resource DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if (WIN32)
    target_link_libraries(main PRIVATE glfw opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(main PRIVATE glfw OpenGL::GL)
endif()
