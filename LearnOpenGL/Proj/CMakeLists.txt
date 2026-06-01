cmake_minimum_required(VERSION 3.17)
project(learn_opengl_proj)

set(CMAKE_CXX_STANDARD 17)

# 头文件路径
include_directories(${CMAKE_SOURCE_DIR}/includes)

# 让 CMake 自动编译 GLFW（从你已有的 Depends/glfw-3.4）
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Depends/glfw-3.4 glfw-build)

# 可执行文件：包含 glad.c + main.cpp
add_executable(main src/glad.c src/main.cpp)

# 链接 GLFW + Windows 的 OpenGL 库
target_link_libraries(main glfw opengl32)
