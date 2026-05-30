cmake_minimum_required(VERSION 3.20)
project(GraphicsEngine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)

FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
)

FetchContent_MakeAvailable(imgui)

set(IMGUI_EXECUTABLES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

find_package(OpenMP COMPONENTS C CXX)

add_executable(GraphicsEngine
    src/main.cpp
    src/graphics_engine.cpp
    src/glad.c
    src/fluid_physics/src/core.c
    src/fluid_physics/src/boundaries.c
    src/fluid_physics/src/scenarios.c
    src/fluid_physics/src/preconditioners.c
    ${IMGUI_EXECUTABLES}
)

target_include_directories(GraphicsEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fluid_physics/src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(GraphicsEngine PRIVATE glfw)

if(OpenMP_C_FOUND)
    target_link_libraries(GraphicsEngine PRIVATE OpenMP::OpenMP_C)
endif()
if(OpenMP_CXX_FOUND)
    target_link_libraries(GraphicsEngine PRIVATE OpenMP::OpenMP_CXX)
endif()
