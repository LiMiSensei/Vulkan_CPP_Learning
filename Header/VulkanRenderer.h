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

class VulkanRenderer
{
public:
    VulkanRenderer();

    int init(GLFWwindow* window);
    void draw();
    void cleanup();
    void updateModel(int modelId, glm::mat4 newModel);
    ~VulkanRenderer();

private:
    GLFWwindow* window;
    //draw
    int currentFrame = 0;
    const bool validationEnabled = true;
    std::vector<Mesh> meshList;
    struct UBO_VP
    {
        glm::mat4 projection;
        glm::mat4 view;
    } ubo_VP;


    //1-实例化Vulkan
    VkInstance instance;

    //2-调试
    VkDebugReportCallbackEXT callback;

    // - 创建表面
    VkSurfaceKHR surface;

    //4-获取物理设备
    VkPhysicalDevice physicalDevice;
    VkDeviceSize minUniformBufferOffset;
    //5-创建逻辑设备
    VkDevice logicalDevice;
    VkQueue graphicsQueue;
    VkQueue presentationQueue;

    //6-创建交换链
    VkSwapchainKHR swapchain;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<SwapchainImage_u> swapChainImages;

    //7-创建渲染Pass
    VkRenderPass renderPass;

    //8-创建描述符集布局
    VkDescriptorSetLayout descriptorSetLayout;

    //9-创建图形管线
    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;

    //10-帧缓冲
    std::vector<VkFramebuffer> swapChainFramebuffers;

    //11-命令池
    VkCommandPool graphicsCommandPool;

    //13-排布每个模型的矩阵
    size_t modelUniformAlignment;
    UboModel* modelTransferSpace;

    //14-在GPU侧建缓冲区
    std::vector<VkBuffer> vpUniformBuffer;
    std::vector<VkDeviceMemory> vpUniformBufferMemory;
    std::vector<VkBuffer> modelDUniformBuffer;
    std::vector<VkDeviceMemory> modelDUniformBufferMemory;

    //15-创建描述符池
    VkDescriptorPool descriptorPool;

    //16-创建描述符集
    std::vector<VkDescriptorSet> descriptorSets;

    //17-录制命令
    std::vector<VkCommandBuffer> commandBuffers;
    // 每个命令缓冲区最后一次提交所用的栅栏（重录前需要等它执行完）
    std::vector<VkFence> commandBufferFences;

    //18-draw
    std::vector<VkSemaphore> imageAvailable; //18-信号量和栅栏
    std::vector<VkSemaphore> renderFinished; //18-信号量和栅栏
    std::vector<VkFence> drawFences;         //18-信号量和栅栏




    //--Vulkan函数：
    void createInstance_1();            //-创建函数
    void createDebugCallback_2();
    void createSurface_3();             //-创建表面
    void getPhysicalDevice_4();         //-枚举机器上的物理设备（显卡），筛出支持图形队列、所需扩展和交换链
    void createLogicalDevice_5();       //-创建逻辑设备
    void createSwapChain_6();           //-创建交换链
    void createRenderPass_7();          //-创建Pass
    void createGraphicsPipeline_9();    //-创建图形管线
    void createFramebuffers_10();       //-帧缓冲
    void createCommandPool_11();        //-命令池
    void createCommandBuffers_12();     //-命令缓冲区
    void recordCommands_17(uint32_t imageIndex); //-重新录制指定交换链图像对应的命令缓冲区
    void createSynchronisation_18();    //-创建信号量
    void createUniformBuffers_14();     //-在 GPU 侧建缓冲区
    void createDescriptorPool_15();     //-创建描述符池
    void createDescriptorSets_16();     //-创建描述符集
    void createDescriptorSetLayout_8(); //-创建描述符集布局

    void allocateDynamicBufferTransferSpace_13(); //-排布每个模型的矩阵
    //ToDo:--检查拓展------------------------------------------------------------------------------------
    bool checkInstanceExtensionSupport_1_(std::vector<const char*>* checkExtensions); //-检查实例拓展
    bool checkDeviceExtensionSupport_A_(VkPhysicalDevice device); // 检查设备拓展
    bool checkValidationLayerSupport();
    bool checkDeviceSuitable_4_A(VkPhysicalDevice device); //-检查设备合适

    //ToDo:--获取函数------------------------------------------------------------------------------------
    //— 查某块设备的队列家族，定位支持图形命令的 graphicsFamily 和支持呈现到窗口的 presentFamily 索引
    QueueFamilyIndices_u getQueueFamilies_56A_(VkPhysicalDevice device);
    //— 查该设备在当前表面上的能力：可用像素格式、呈现模式和分辨率范围，后面挑选参数靠它
    SwapChainDetails_u getSwapChainDetails_A6(VkPhysicalDevice device);

    //ToDo:--选择函数------------------------------------------------------------------------------------
    //从可用格式里挑最合适的：优先 R8G8B8A8 / B8G8R8A8 加 sRGB 色彩空间，否则退回第一个
    VkSurfaceFormatKHR chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    //挑呈现模式：优先 MAILBOX（三缓冲、低延迟），找不到就退回规范强制支持的 FIFO
    VkPresentModeKHR chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes);
    //决定交换链图像分辨率：能力给了固定值就用它，否则取窗口帧缓冲大小并夹到 min/max 范围内
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    //ToDo:--创建函数------------------------------------------------------------------------------------
    //为图像创建 2D 视图。交换链拿到的是裸图像，必须包一层视图才能被帧缓冲和管线使用
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    //把读入的 SPIR-V 字节码包装成着色器模块，作为图形管线各阶段的可执行代码来源
    VkShaderModule createShaderModule(const std::vector<char>& code);


    void updateUniformBuffers(uint32_t imageIndex); //-更新统一缓冲区

    //--ImGui函数：
    void initImGui();       //-初始化 ImGui（GLFW 平台后端 + Vulkan 渲染后端）
    void destroyImGui();    //-关闭并销毁 ImGui
    void drawImGui();       //-构建界面内容（在 ImGui::NewFrame 之后、ImGui::Render 之前调用）
};
