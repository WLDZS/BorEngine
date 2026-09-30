#include "VulkanRuntime.h"

#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <print>
#include <stdexcept>
#include <vector>

namespace
{
    std::vector<uint32_t> ReadShaderCode(const std::filesystem::path &path)
    {
        // 二进制模式读取；打开时停在末尾，便于取得文件大小。
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            throw std::runtime_error("无法打开着色器文件：" + path.string());

        const std::streamoff byteSize = file.tellg();
        if (byteSize <= 0 || byteSize % sizeof(uint32_t) != 0)
            throw std::runtime_error("着色器文件大小必须是非零的 4 字节倍数：" + path.string());

        // Vulkan 接收 uint32_t 数组，使用它存储可保证正确的内存对齐。
        std::vector<uint32_t> code(static_cast<size_t>(byteSize) / sizeof(uint32_t));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char *>(code.data()), byteSize))
            throw std::runtime_error("读取着色器文件失败：" + path.string());

        return code;
    }
}

namespace Bor3D
{
    bool VulkanRuntime::Init(GLFWwindow *window)
    {
        if (window == nullptr || static_cast<bool>(*instance))
            return false;

        if (glfwVulkanSupported() == GLFW_FALSE)
        {
            std::println(stderr, "当前环境不支持 Vulkan");
            return false;
        }

        try
        {
            CreateInstance();
            CreateSurface(window);
            PickPhysicalDevice();
            CreateDevice();
            CreateShaderModules();
            CreateSwapchain(window);
            CreateImageViews();
            CreateCommandPool();
            CreateCommandBuffer();
            CreateSyncObjects();
            return true;
        }
        catch (const std::exception &error)
        {
            std::println(stderr, "Vulkan 初始化失败：{}", error.what());
            Shutdown();
            return false;
        }
    }

    void VulkanRuntime::Shutdown()
    {
        if (static_cast<bool>(*device))
            device.waitIdle();

        inFlightFence.clear();
        renderFinishedSemaphores.clear();
        imageAvailableSemaphore.clear();
        commandBuffer.clear();
        commandPool.clear();
        swapchainImageViews.clear();
        swapchainImages.clear();
        swapchain.clear();

        graphicsQueue.clear();
        fragmentShaderModule.clear();
        vertexShaderModule.clear();
        device.clear();
        physicalDevice.clear();
        surface.clear();
        instance.clear();

        queueFamilyIndex = 0;
    }

    bool VulkanRuntime::DrawFrame() const
    {
        constexpr uint64_t timeout = std::numeric_limits<uint64_t>::max();

        // CPU 等待上一帧的 GPU 工作完成。
        if (device.waitForFences(*inFlightFence, vk::True, timeout)
            != vk::Result::eSuccess)
            throw std::runtime_error("等待上一帧完成失败");

        try
        {
            // 获取本帧可以使用的交换链图像。
            const vk::ResultValue<uint32_t> acquired =
                swapchain.acquireNextImage(
                    timeout, *imageAvailableSemaphore, nullptr);

            if (acquired.result != vk::Result::eSuccess &&
                acquired.result != vk::Result::eSuboptimalKHR)
                throw std::runtime_error("获取交换链图像失败");

            const uint32_t imageIndex = acquired.value;

            RecordCommandBuffer(imageIndex);
            SubmitCommandBuffer(imageIndex);
            return PresentImage(imageIndex);
        }
        catch (const vk::OutOfDateKHRError&)
        {
            std::println("交换链已失效，需要重建；本次停止渲染");
            return false;
        }
    }

    void VulkanRuntime::PickPhysicalDevice()
    {
        physicalDevice.clear();

        const std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        if (devices.empty())
            throw std::runtime_error("未找到支持 Vulkan 的 GPU");

        int bestPriority = -1;
        for (const vk::raii::PhysicalDevice &candidate : devices)
        {
            const std::vector<vk::ExtensionProperties> extensions = candidate.enumerateDeviceExtensionProperties();
            const bool supportsSwapchain = std::ranges::any_of(
                    extensions, [](const vk::ExtensionProperties &extension)
                    { return std::strcmp(extension.extensionName, vk::KHRSwapchainExtensionName) == 0; });
            if (!supportsSwapchain)
                continue;

            const vk::PhysicalDeviceProperties properties = candidate.getProperties();
            const std::vector<vk::QueueFamilyProperties> queueFamilies = candidate.getQueueFamilyProperties();
            for (uint32_t i = 0; i < queueFamilies.size(); i++)
            {
                const bool graphics = static_cast<bool>(queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics);
                const bool present = candidate.getSurfaceSupportKHR(i, *surface);
                if (!graphics || !present)
                    continue;

                const int priority = properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 1 : 0;
                if (priority > bestPriority)
                {
                    physicalDevice = candidate;
                    queueFamilyIndex = i;
                    bestPriority = priority;
                }
                break;
            }
        }

        if (bestPriority < 0)
            throw std::runtime_error("未找到同时支持绘图、窗口呈现和 VK_KHR_swapchain 的 GPU");

        std::println("已选择 GPU：{}（队列族：{}）", physicalDevice.getProperties().deviceName.data(),
                     queueFamilyIndex);
    }

    void VulkanRuntime::CreateShaderModules()
    {
        // CMake 提供当前构建目录，运行时不依赖 CLion 的工作目录。
        const std::filesystem::path shaderDirectory = BOR_ENGINE_SHADER_DIR;
        const std::vector<uint32_t> vertexCode = ReadShaderCode(shaderDirectory / "triangle.vert.spv");
        const std::vector<uint32_t> fragmentCode = ReadShaderCode(shaderDirectory / "triangle.frag.spv");

        const vk::ShaderModuleCreateInfo vertexCreateInfo{
            .codeSize = vertexCode.size() * sizeof(uint32_t),
            .pCode = vertexCode.data()
        };
        const vk::ShaderModuleCreateInfo fragmentCreateInfo{
            .codeSize = fragmentCode.size() * sizeof(uint32_t),
            .pCode = fragmentCode.data()
        };

        vertexShaderModule = vk::raii::ShaderModule(device, vertexCreateInfo);
        fragmentShaderModule = vk::raii::ShaderModule(device, fragmentCreateInfo);
        std::println("顶点和片元着色器模块创建成功");
    }

    void VulkanRuntime::CreateSwapchain(GLFWwindow *window)
    {
        const vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        const std::vector<vk::SurfaceFormatKHR> formats = physicalDevice.getSurfaceFormatsKHR(*surface);
        const std::vector<vk::PresentModeKHR> presentModes = physicalDevice.getSurfacePresentModesKHR(*surface);

        //std::println("交换链最少图像数：{}", capabilities.minImageCount);
        //std::println("窗口表面当前尺寸：{} x {}", capabilities.currentExtent.width, capabilities.currentExtent.height);
        //std::println("可用图像格式数：{}", formats.size());
        //std::println("可用呈现模式数：{}", presentModes.size());

        uint32_t imageCount = capabilities.minImageCount + 1;

        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        std::println("计划请求的交换链图像数：{}", imageCount);

        if (formats.empty())
            throw std::runtime_error("未找到可用的窗口表面图像格式");

        vk::SurfaceFormatKHR surfaceFormat = formats.front();

        for (const vk::SurfaceFormatKHR &format : formats)
        {
            if (format.format == vk::Format::eB8G8R8A8Srgb &&
                format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                surfaceFormat = format;
                break;
            }
        }

        if (presentModes.empty())
            throw std::runtime_error("未找到可用的呈现模式");

        constexpr vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;

        vk::Extent2D extent = capabilities.currentExtent;

        if (extent.width == std::numeric_limits<uint32_t>::max())
        {
            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);

            extent.width = std::clamp(
                static_cast<uint32_t>(width),
                capabilities.minImageExtent.width,
                capabilities.maxImageExtent.width);

            extent.height = std::clamp(
                static_cast<uint32_t>(height),
                capabilities.minImageExtent.height,
                capabilities.maxImageExtent.height);
        }

        std::println("选择的图像尺寸：{} x {}", extent.width, extent.height);
        std::println("选择的呈现模式：{}", vk::to_string(presentMode));
        std::println("选择的图像格式：{}",vk::to_string(surfaceFormat.format));
        std::println("选择的色彩空间：{}",vk::to_string(surfaceFormat.colorSpace));

        const vk::SwapchainCreateInfoKHR createInfo{
            .surface = *surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,

            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = presentMode,
            .clipped = vk::True
        };

        swapchain = vk::raii::SwapchainKHR(device, createInfo);

        swapchainImages = swapchain.getImages();
        swapchainSurfaceFormat = surfaceFormat;
        swapchainExtent = extent;

        std::println("交换链创建成功，实际图像数量：{}",swapchain.getImages().size());
    }

    void VulkanRuntime::CreateInstance()
    {
        uint32_t extensionCount = 0;
        const char **extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        if (extensions == nullptr || extensionCount == 0)
            throw std::runtime_error("获取 GLFW 所需的 Vulkan 实例扩展失败");

        const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};

        constexpr vk::ApplicationInfo appInfo{.pApplicationName = "BorEngine", .apiVersion = vk::ApiVersion14};

        const vk::InstanceCreateInfo createInfo{.pApplicationInfo = &appInfo,
                                                .enabledLayerCount = 1,
                                                .ppEnabledLayerNames = validationLayers,
                                                .enabledExtensionCount = extensionCount,
                                                .ppEnabledExtensionNames = extensions};

        instance = vk::raii::Instance(context, createInfo);
    }

    void VulkanRuntime::CreateDevice()
    {
        if (physicalDevice.getProperties().apiVersion < vk::ApiVersion13)
            throw std::runtime_error("当前渲染流程要求 GPU 支持 Vulkan 1.3 或更高版本");

        const vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan13Features
        > supportedFeatures =
            physicalDevice.getFeatures2<
                vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan13Features
            >();

        const auto& supportedVulkan13 =
            supportedFeatures.get<vk::PhysicalDeviceVulkan13Features>();

        if (supportedVulkan13.dynamicRendering == vk::False)
            throw std::runtime_error("所选 GPU 不支持动态渲染");

        if (supportedVulkan13.synchronization2 == vk::False)
            throw std::runtime_error("所选 GPU 不支持 synchronization2 同步功能");

        constexpr float queuePriority = 1.0f;
        const vk::DeviceQueueCreateInfo queueCreateInfo{
                .queueFamilyIndex = queueFamilyIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};

        const char *deviceExtensions[] = {vk::KHRSwapchainExtensionName};

        constexpr vk::PhysicalDeviceVulkan13Features enabledVulkan13{
            .synchronization2 = vk::True,
            .dynamicRendering = vk::True
        };

        const vk::DeviceCreateInfo deviceCreateInfo{.pNext = &enabledVulkan13,
                                                    .queueCreateInfoCount = 1,
                                                    .pQueueCreateInfos = &queueCreateInfo,
                                                    .enabledExtensionCount = 1,
                                                    .ppEnabledExtensionNames = deviceExtensions};

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, queueFamilyIndex, 0);
        std::println("逻辑设备创建成功（队列族：{}）", queueFamilyIndex);
    }

    void VulkanRuntime::CreateSurface(GLFWwindow *window)
    {
        VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
        if (const VkResult result = glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface);
            result != VK_SUCCESS)
            throw std::runtime_error("Vulkan 窗口表面创建失败，错误码：" +
                                     std::to_string(result));

        surface = vk::raii::SurfaceKHR(instance, rawSurface);
    }

    void VulkanRuntime::CreateImageViews()
    {
        for (const vk::Image &image : swapchainImages)
        {
            const vk::ImageViewCreateInfo createInfo{
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = swapchainSurfaceFormat.format,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };

            swapchainImageViews.emplace_back(device, createInfo);
        }

        std::println("图像视图创建成功，数量：{}",swapchainImageViews.size());
    }

    void VulkanRuntime::CreateCommandPool()
    {
        const vk::CommandPoolCreateInfo createInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueFamilyIndex
        };

        commandPool = vk::raii::CommandPool(device, createInfo);

        std::println("命令池创建成功");
    }

    void VulkanRuntime::CreateCommandBuffer()
    {
        const vk::CommandBufferAllocateInfo allocateInfo{
            .commandPool = *commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };

        vk::raii::CommandBuffers buffers(device, allocateInfo);
        commandBuffer = std::move(buffers.front());

        std::println("命令缓冲区分配成功");
    }

    void VulkanRuntime::CreateSyncObjects()
    {
        constexpr vk::SemaphoreCreateInfo semaphoreInfo{};

        imageAvailableSemaphore =vk::raii::Semaphore(device, semaphoreInfo);

        renderFinishedSemaphores.reserve(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++)
        {
            renderFinishedSemaphores.emplace_back(device, semaphoreInfo);
        }

        constexpr vk::FenceCreateInfo fenceInfo{
            .flags = vk::FenceCreateFlagBits::eSignaled
        };

        inFlightFence = vk::raii::Fence(device, fenceInfo);
    }

    void VulkanRuntime::RecordCommandBuffer(uint32_t imageIndex) const
    {
        commandBuffer.reset();
        commandBuffer.begin(vk::CommandBufferBeginInfo{});

        constexpr vk::ImageSubresourceRange range{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        // 将图像切换为可作为颜色附件使用的布局。
        const vk::ImageMemoryBarrier2 toColor{
            .srcStageMask = vk::PipelineStageFlagBits2::eNone,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchainImages[imageIndex],
            .subresourceRange = range
        };

        const vk::DependencyInfo beforeRendering{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &toColor
        };

        commandBuffer.pipelineBarrier2(beforeRendering);

        // 指定要清除的图像视图和背景颜色。
        const vk::RenderingAttachmentInfo colorAttachment{
            .imageView = *swapchainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                vk::ClearColorValue{0.1f, 0.2f, 0.4f, 1.0f}
            }
        };

        const vk::RenderingInfo renderingInfo{
            .renderArea = {.offset = {0, 0}, .extent = swapchainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment
        };

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.endRendering();

        // 清屏结束，将图像切换为可呈现的布局。
        const vk::ImageMemoryBarrier2 toPresent{
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eNone,
            .dstAccessMask = vk::AccessFlagBits2::eNone,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchainImages[imageIndex],
            .subresourceRange = range
        };

        const vk::DependencyInfo afterRendering{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &toPresent
        };

        commandBuffer.pipelineBarrier2(afterRendering);

        commandBuffer.end();
    }

    void VulkanRuntime::SubmitCommandBuffer(uint32_t imageIndex) const
    {
        // GPU 写入颜色附件之前，等待图像可用。
        const vk::SemaphoreSubmitInfo waitInfo{
            .semaphore = *imageAvailableSemaphore,
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };

        const vk::CommandBufferSubmitInfo commandInfo{
            .commandBuffer = *commandBuffer
        };

        // 使用这张交换链图像对应的呈现信号量。
        const vk::Semaphore renderFinished =
            *renderFinishedSemaphores[imageIndex];

        const vk::SemaphoreSubmitInfo signalInfo{
            .semaphore = renderFinished,
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands
        };

        const vk::SubmitInfo2 submitInfo{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitInfo,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalInfo
        };

        // 把 Fence 恢复为“未完成”，交给本次提交再次触发。
        device.resetFences(*inFlightFence);
        graphicsQueue.submit2(submitInfo, *inFlightFence);
    }

    bool VulkanRuntime::PresentImage(uint32_t imageIndex) const
    {
        // 等待渲染完成，然后呈现这张图像。
        const vk::Semaphore renderFinished =
            *renderFinishedSemaphores[imageIndex];
        const vk::SwapchainKHR swapchainHandle = *swapchain;

        const vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderFinished,
            .swapchainCount = 1,
            .pSwapchains = &swapchainHandle,
            .pImageIndices = &imageIndex
        };

        const vk::Result presentResult =
            graphicsQueue.presentKHR(presentInfo);

        return presentResult == vk::Result::eSuccess ||
               presentResult == vk::Result::eSuboptimalKHR;
    }
} // namespace Bor3D
