#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <array>

#include "Mesh.h"
#include "VulkanValidation.h"
#include "Utilities.h"



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
    void updateModel(int modelId, glm::mat4 newModel);
    ~VulkanRenderer();

private:
    GLFWwindow* window;
    int currentFrame = 0;
    const bool validationEnabled = true;
    struct UboViewProjection {
        glm::mat4 projection;
        glm::mat4 view;
    } uboViewProjection;



    // - 实例化Vulkan
    VkInstance instance;
    VkDebugReportCallbackEXT callback;

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
    VkCommandPool graphicsCommandPool;               //10-命令池
    std::vector<VkFramebuffer> swapChainFramebuffers;//9-帧缓冲
    std::vector<VkCommandBuffer> commandBuffers;     //11-命令缓冲区

    // - 录制命令
    std::vector<Mesh> meshList;                     //12-录制命令

    // - 信号量
    std::vector<VkSemaphore> imageAvailable;        //13-信号量和栅栏
    std::vector<VkSemaphore> renderFinished;        //13-信号量和栅栏
    std::vector<VkFence> drawFences;                //13-信号量和栅栏

    VkDeviceSize minUniformBufferOffset;
    size_t modelUniformAlignment;
    UboModel * modelTransferSpace;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;

    std::vector<VkBuffer> vpUniformBuffer;
    std::vector<VkDeviceMemory> vpUniformBufferMemory;

    std::vector<VkBuffer> modelDUniformBuffer;
    std::vector<VkDeviceMemory> modelDUniformBufferMemory;

    //--Vulkan函数：
    void createInstance_1();            //-创建函数
    void createDebugCallback_2();
    void createSurface_3();             //-创建表面
    void getPhysicalDevice_4();         //-枚举机器上的物理设备（显卡），筛出支持图形队列、所需扩展和交换链
    void createLogicalDevice_5();       //-创建逻辑设备

    void createSwapChain_6();           //-创建交换链
    void createRenderPass_7();          //-创建Pass
    void createGraphicsPipeline_8();    //-创建图形管线

    void createFramebuffers_9();        //-帧缓冲
    void createCommandPool_10();        //-命令池
    void createCommandBuffers_11();     //-命令缓冲区
    void recordCommands_12();           //-录制命令

    void createSynchronisation_13();    //-创建信号量
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createDescriptorSetLayout();
    void updateUniformBuffers(uint32_t imageIndex);
    void allocateDynamicBufferTransferSpace();
    //ToDo:--检查拓展------------------------------------------------------------------------------------
    bool checkInstanceExtensionSupport_1_(std::vector<const char *>* checkExtensions);  //-检查实例拓展
    bool checkDeviceExtensionSupport_A_(VkPhysicalDevice device);                       // 检查设备拓展
    bool checkValidationLayerSupport();
    bool checkDeviceSuitable_4_A(VkPhysicalDevice device);                              //-检查设备合适

    //ToDo:--获取函数------------------------------------------------------------------------------------
    //— 查某块设备的队列家族，定位支持图形命令的 graphicsFamily 和支持呈现到窗口的 presentFamily 索引
    QueueFamilyIndices_u getQueueFamilies_56A_(VkPhysicalDevice device);
    //— 查该设备在当前表面上的能力：可用像素格式、呈现模式和分辨率范围，后面挑选参数靠它
    SwapChainDetails_u getSwapChainDetails_A6(VkPhysicalDevice device);

    //ToDo:--选择函数------------------------------------------------------------------------------------
    //从可用格式里挑最合适的：优先 R8G8B8A8 / B8G8R8A8 加 sRGB 色彩空间，否则退回第一个
    VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
    //挑呈现模式：优先 MAILBOX（三缓冲、低延迟），找不到就退回规范强制支持的 FIFO
    VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
    //决定交换链图像分辨率：能力给了固定值就用它，否则取窗口帧缓冲大小并夹到 min/max 范围内
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    //ToDo:--创建函数------------------------------------------------------------------------------------
    //为图像创建 2D 视图。交换链拿到的是裸图像，必须包一层视图才能被帧缓冲和管线使用
    VkImageView createImageView(VkImage image,VkFormat format,VkImageAspectFlags aspectFlags);
    //把读入的 SPIR-V 字节码包装成着色器模块，作为图形管线各阶段的可执行代码来源
    VkShaderModule createShaderModule(const std::vector<char> &code);
};


