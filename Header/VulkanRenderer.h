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
    void deaw();
    void cleanup();

    ~VulkanRenderer();

private:
    GLFWwindow* window;
    int currentFrame = 0;
    const bool validationEnabled = true;
    // - 实例化Vulkan
    VkInstance instance;
    VkDebugReportCallbackEXT debugCallback;

    // - 创建逻辑设备
    struct {//代表主设备
        VkPhysicalDevice physicalDevice;
        VkDevice logicalDevice;
    } mainDevice;                                   //5-创建逻辑设备
    VkQueue graphicsQueue;                          //5-创建逻辑设备
    VkQueue presentationQueue;                      //5-创建逻辑设备

    // - 创建表面
    VkSurfaceKHR surface;                           //3-创建表面

    // - 交换链
    VkSwapchainKHR swapchain;                       //6-创建交换链
    VkFormat swapChainImageFormat;                  //6-创建交换链
    VkExtent2D swapChainExtent;                     //6-创建交换链
    std::vector<SwapchainImage_u> swapChainImages;  //6-创建交换链

    // - 管线
    VkRenderPass renderPass;                        //7-创建渲染Pass
    VkPipeline graphicsPipeline;                    //8-创建图形管线
    VkPipelineLayout pipelineLayout;                //8-创建图形管线

    // - 帧缓存与命令缓冲
    std::vector<VkFramebuffer> swapChainFramebuffers;//9-帧缓冲
    VkCommandPool graphicsCommandPool;               //10-命令池
    std::vector<VkCommandBuffer> commandBuffers;     //11-命令缓冲区

    // - 信号量
    VkSemaphore imageAvailable;                      //13-信号量和栅栏
    VkSemaphore renderFinished;                      //13-信号量和栅栏
    std::vector<VkFence> drawFences;





    //--Vulkan函数：
    void createInstance_1();            //-创建函数
    void createDebugCallback_2();
    void createSurface_3();             //-创建表面
    void createLogicalDevice_5();       //-创建逻辑设备

    void createSwapChain_6();           //-创建交换链
    void createRenderPass_7();          //-创建Pass
    void createGraphicsPipeline_8();    //-创建图形管线

    void createFramebuffers_9();        //-帧缓冲
    void createCommandPool_10();        //-命令池
    void createCommandBuffers_11();     //-命令缓冲区
    void recordCommands_12();           //-录制命令

    void createSynchronisation_13();    //-创建信号量



    //--检查拓展：
    bool checkInstanceExtensionSupport_1_(std::vector<const char *>* checkExtensions);  //-检查实例拓展
    bool checkDeviceExtensionSupport_A_(VkPhysicalDevice device);                       // 检查设备拓展
    bool checkValidationLayerSupport();
    bool checkDeviceSuitable_4_A(VkPhysicalDevice device);                              //-检查设备合适

    //--获取函数：
    void getPhysicalDevice_4();
    QueueFamilyIndices_u getQueueFamilies_56A_(VkPhysicalDevice device);
    SwapChainDetails_u getSwapChainDetails_A6(VkPhysicalDevice device);

    //--选择函数
    VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
    VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    //--创建函数
    VkImageView createImageView(VkImage image,VkFormat format,VkImageAspectFlags aspectFlags);
    VkShaderModule createShaderModule(const std::vector<char> &code);
};


#endif //VULKAN_CPP_LEARNING_VULKANRENDERER_H
