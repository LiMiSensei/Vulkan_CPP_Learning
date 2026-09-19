//
// Created by LiMi on 2026/9/14.
//

#ifndef VULKAN_CPP_LEARNING_VULKANRENDERER_H
#define VULKAN_CPP_LEARNING_VULKANRENDERER_H


// windows.h 会定义 min/max 宏，破坏 std::numeric_limits<T>::max() 等写法，需在包含前屏蔽
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define VK_USE_PLATFORM_WIN32_KHR //#include <windows.h> //#include "vulkan_win32.h"




#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>


#include "Utilities.h"

class VulkanRenderer {
public:
    VulkanRenderer();

    int init(GLFWwindow* window);

    void cleanup();

    ~VulkanRenderer();

private:
    GLFWwindow* window;
    //Vulkan组件
    // - Main
    VkInstance instance;
    VkDebugReportCallbackEXT debugCallback;
    struct {//代表主设备
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;

    VkQueue graphicsQueue;
    VkQueue presentationQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<SwapchainImage_u> swapChainImages;
    // - Pipekine
    VkPipelineLayout pipelineLayout;


    // - Utility
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;


    //Vulkan函数：
    void createInstance_1();//-创建函数
    void createDebugCallback_2();
    void createSurface_3();
    void createLogicalDevice_5();//-创建逻辑设备
    void createSwapChain_6();
    void createRenderPass();//-创建渲染Pass
    void createGraphicsPipeline();//-创建图形管线

    //--检查拓展：
    bool checkInstanceExtensionSupport_1_(std::vector<const char *>* checkExtensions);//-检查设备拓展
    bool checkDeviceExtensionSupport_A_(VkPhysicalDevice device);
    bool checkValidationLayerSupport();
    bool checkDeviceSuitable_4_A(VkPhysicalDevice device);//-检查设备

    //--获取函数：
    void getPhysicalDevice_4();
    QueueFamilyIndices_u getQueueFamilies_56A_(VkPhysicalDevice device);
    SwapChainDetails_u getSwapChainDetails_A6(VkPhysicalDevice device);

    //--选择函数
    VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
    VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    //创建函数
    VkImageView createImageView(VkImage image,VkFormat format,VkImageAspectFlags aspectFlags);
    VkShaderModule createShaderModule(const std::vector<char> &code);
};


#endif //VULKAN_CPP_LEARNING_VULKANRENDERER_H
