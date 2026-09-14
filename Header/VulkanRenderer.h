//
// Created by LiMi on 2026/9/14.
//

#ifndef VULKAN_CPP_LEARNING_VULKANRENDERER_H
#define VULKAN_CPP_LEARNING_VULKANRENDERER_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>

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
    struct {
        VkPhysicalDevice physicalDevice;//代表主设备
        VkDevice device;
    } mainDevice;


    //Vulkan函数：
    //-创建函数
    void createInstance();
    //-获取函数
    void getPhysicalDevice();
    //-检查拓展
    bool checkInstanceExtensionSupport(std::vector<const char*>* checkExtensions);
    bool checkDeviceSuitable(VkPhysicalDevice device);
};


#endif //VULKAN_CPP_LEARNING_VULKANRENDERER_H
