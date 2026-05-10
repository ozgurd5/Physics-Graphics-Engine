cmake_minimum_required(VERSION 4.2)
project(GraphicsEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

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

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.5
)
FetchContent_MakeAvailable(imgui)

find_package(OpenMP)

add_executable(GraphicsEngine
        src/main.cpp
        src/graphics_engine.cpp
        src/glad.c
        src/fluid_graphics/core.c
        src/fluid_graphics/boundaries.c
        src/fluid_graphics/scenarios.c
        src/fluid_graphics/preconditioners.c
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        include/glad/glad.h
        include/KHR/khrplatform.h)

target_include_directories(GraphicsEngine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fluid_graphics
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends)

target_link_libraries(GraphicsEngine PRIVATE glfw glm::glm)

if(OpenMP_C_FOUND)
    target_link_libraries(GraphicsEngine PRIVATE OpenMP::OpenMP_C)
endif()
if(OpenMP_CXX_FOUND)
    target_link_libraries(GraphicsEngine PRIVATE OpenMP::OpenMP_CXX)
endif()
