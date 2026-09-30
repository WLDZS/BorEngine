//
// Created by abc on 2026/9/27.
//

#include "Application.h"

#include <print>

namespace Bor3D
{
    bool Application::Init()
    {
        if (!window.Init(800, 600, "BorEngine"))
            return false;

        if (!vulkanRuntime.Init(window.GetHandle()))
            return false;

        std::println("应用程序初始化成功！");
        return true;
    }

    void Application::Run() const
    {
        while (!window.ShouldClose())
        {
            window.PollEvents();

            if (window.ShouldClose())
                break;

            if (!vulkanRuntime.DrawFrame())
                break;
        }
    }

    Application::~Application()
    {
        vulkanRuntime.Shutdown();
        window.Shutdown();
    }
} // namespace Bor3D
