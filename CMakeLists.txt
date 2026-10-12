cmake_minimum_required(VERSION 3.21)
project(SeventhNight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)
find_path(CGLTF_INCLUDE_DIRS "cgltf.h")
find_path(STB_INCLUDE_DIRS "stb_image.h")

add_executable(SeventhNight
    src/main.cpp
    src/Model.cpp
    src/core/Assets.cpp
    src/anim/Animation.cpp
    src/anim/Animator.cpp
    src/anim/SkinnedModel.cpp
    src/physics/PhysicsEngine.cpp
    src/audio/AudioEngine.cpp
)

target_include_directories(SeventhNight PRIVATE
    "C:/vcpkg/installed/x64-windows/include"
    ${CGLTF_INCLUDE_DIRS}
    ${STB_INCLUDE_DIRS}
    src
)

target_link_libraries(SeventhNight PRIVATE
    glfw
    GLEW::GLEW
    glm::glm
    winmm
)

set_target_properties(SeventhNight PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if (MSVC)
    target_compile_options(SeventhNight PRIVATE /W3)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SeventhNight)
endif()
