cmake_minimum_required(VERSION 4.3)
project(BorEngine)

set(CMAKE_CXX_STANDARD 26)

add_executable(BorEngine main.cpp
        Application.cpp
        Application.h
)
add_subdirectory(ThirdParty/glfw-3.5.1)

target_link_libraries(BorEngine PRIVATE glfw)
