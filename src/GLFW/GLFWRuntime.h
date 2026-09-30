#ifndef BORENGINE_GLFW_RUNTIME_H
#define BORENGINE_GLFW_RUNTIME_H

#include "GLFW/glfw3.h"

namespace Bor3D
{
    class GLFWRuntime
    {
    public:
        GLFWRuntime() = default;
        GLFWRuntime(const GLFWRuntime &) = delete;
        GLFWRuntime &operator=(const GLFWRuntime &) = delete;
        ~GLFWRuntime();

        bool Init(int width, int height, const char *title);
        void Shutdown();
        void PollEvents() const;

        bool ShouldClose() const;
        void WaitEvents() const;
        GLFWwindow *GetHandle() const;

    private:
        GLFWwindow *window = nullptr;
        bool glfwInitialized = false;
    };
} // namespace Bor3D

#endif // BORENGINE_GLFW_RUNTIME_H
