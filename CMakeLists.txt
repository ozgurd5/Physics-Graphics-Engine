cmake_minimum_required(VERSION 4.2)
project(GraphicsEngine)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

add_executable(GraphicsEngine
        src/main.cpp
        src/graphics_engine.cpp
        src/simulation_engine.c
        src/glad.c
        include/glad/glad.h
        include/KHR/khrplatform.h)

target_include_directories(GraphicsEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(GraphicsEngine PRIVATE glfw glm::glm)