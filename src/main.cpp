#include "Application.h"
#include <cstdio>
#include <exception>
#include <print>

int main()
{
    try
    {
        Bor3D::Application app;

        if (!app.Init())
        {
            std::println(stderr, "BorEngine 初始化失败");
            return 1;
        }
        app.Run();
        return 0;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "BorEngine 发生错误：{}", error.what());
        return 1;
    }
}
