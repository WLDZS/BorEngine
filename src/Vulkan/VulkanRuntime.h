#ifndef BORENGINE_VULKAN_RUNTIME_H
#define BORENGINE_VULKAN_RUNTIME_H

#include <vulkan/vulkan_raii.hpp>

struct GLFWwindow;

namespace Bor3D
{
    class VulkanRuntime
    {
    public:
        VulkanRuntime() = default;
        VulkanRuntime(const VulkanRuntime &) = delete;
        VulkanRuntime &operator=(const VulkanRuntime &) = delete;
        ~VulkanRuntime() = default;

        bool Init(GLFWwindow *window);
        void Shutdown();
        bool DrawFrame() const;
    private:
        vk::raii::Context context;
        vk::raii::Instance instance = nullptr;
        vk::raii::SurfaceKHR surface = nullptr;
        vk::raii::PhysicalDevice physicalDevice = nullptr;
        vk::raii::Device device = nullptr;
        vk::raii::ShaderModule vertexShaderModule = nullptr;
        vk::raii::ShaderModule fragmentShaderModule = nullptr;
        vk::raii::Queue graphicsQueue = nullptr;
        uint32_t queueFamilyIndex = 0;
        vk::raii::SwapchainKHR swapchain = nullptr;
        std::vector<vk::Image> swapchainImages;
        std::vector<vk::raii::ImageView> swapchainImageViews;
        vk::SurfaceFormatKHR swapchainSurfaceFormat{};
        vk::Extent2D swapchainExtent{};
        vk::raii::CommandPool commandPool = nullptr;
        vk::raii::CommandBuffer commandBuffer = nullptr;
        vk::raii::Semaphore imageAvailableSemaphore = nullptr;
        std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
        vk::raii::Fence inFlightFence = nullptr;

        void CreateInstance();
        void CreateSurface(GLFWwindow *window);
        void PickPhysicalDevice();
        void CreateDevice();
        void CreateShaderModules();
        void CreateSwapchain(GLFWwindow *window);
        void CreateImageViews();
        void CreateCommandPool();
        void CreateCommandBuffer();
        void CreateSyncObjects();
        void RecordCommandBuffer(uint32_t imageIndex) const;
        void SubmitCommandBuffer(uint32_t imageIndex) const;
        bool PresentImage(uint32_t imageIndex) const;
    };
} // namespace Bor3D
#endif // BORENGINE_VULKAN_RUNTIME_H
