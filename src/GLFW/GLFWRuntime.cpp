#include "GLFWRuntime.h"

#define GLFW_INCLUDE_NONE

#include <print>

namespace Bor3D
{
    bool GLFWRuntime::Init(const int width, const int height, const char *title)
    {
        if (glfwInitialized)
            return false;

        if (glfwInit() == GLFW_FALSE)
        {
            std::println(stderr, "GLFW 初始化失败");
            return false;
        }

        glfwInitialized = true;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (window == nullptr)
        {
            std::println(stderr, "窗口创建失败");
            Shutdown();
            return false;
        }

        return true;
    }

    void GLFWRuntime::Shutdown()
    {
        if (window != nullptr)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }

        if (glfwInitialized)
        {
            glfwTerminate();
            glfwInitialized = false;
        }
    }

    void GLFWRuntime::PollEvents() const
    {
        if (window != nullptr)
            glfwPollEvents();
    }

    bool GLFWRuntime::ShouldClose() const
    {
        return window == nullptr || glfwWindowShouldClose(window) == GLFW_TRUE;
    }

    void GLFWRuntime::WaitEvents() const
    {
        if (window != nullptr)
            glfwWaitEvents();
    }

    GLFWwindow *GLFWRuntime::GetHandle() const
    {
        return window;
    }

    GLFWRuntime::~GLFWRuntime()
    {
        Shutdown();
    }
} // namespace Bor3D
