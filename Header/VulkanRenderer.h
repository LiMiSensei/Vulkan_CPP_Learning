//
// Created by LiMi on 2026/9/14.
//

#ifndef VULKAN_CPP_LEARNING_VULKANRENDERER_H
#define VULKAN_CPP_LEARNING_VULKANRENDERER_H


#define VK_USE_PLATFORM_WIN32_KHR

//#include <windows.h>
//#include "vulkan_win32.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>
#include <set>

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
    VkInstance instance;
    VkDebugReportCallbackEXT debugCallback;
    //代表主设备
    struct {
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;

    VkQueue graphicsQueue;
    VkQueue presentationQueue;
    VkSurfaceKHR surface;

    //Vulkan函数：
    void createInstance_1();//-创建函数
    void createDebugCallback_();
    void createLogicalDevice_3();//-创建逻辑设备
    void createSurface_2();

    //获取函数：
    void getPhysicalDevice_2();

    //检查拓展：
    bool checkInstanceExtensionSupport_(std::vector<const char *>* checkExtensions);//-检查设备拓展
    bool checkDeviceExtensionSupport_(VkPhysicalDevice device);
    bool checkValidationLayerSupport();
    bool checkDeviceSuitable_(VkPhysicalDevice device);//-检查设备

    //获取函数：
    QueueFamilyIndices_ getQueueFamilies_(VkPhysicalDevice device);
};


#endif //VULKAN_CPP_LEARNING_VULKANRENDERER_H
