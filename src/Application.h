//
// Created by abc on 2026/9/27.
//

#ifndef BORENGINE_APPLICATION_H
#define BORENGINE_APPLICATION_H

#include "GLFW/GLFWRuntime.h"
#include "Vulkan/VulkanRuntime.h"

namespace Bor3D
{
    class Application
    {
    public:
        bool Init();
        void Run() const;
        ~Application();

    private:
        GLFWRuntime window;
        VulkanRuntime vulkanRuntime;
    };

} // namespace Bor3D

#endif // BORENGINE_APPLICATION_H
