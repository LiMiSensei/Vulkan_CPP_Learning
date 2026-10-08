//
// Created by LiMi on 2026/9/29.
//

#ifndef VULKAN_CPP_LEARNING_TESTRENDER_H
#define VULKAN_CPP_LEARNING_TESTRENDER_H
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "GLFW/glfw3.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Test_ImguiVulaknBackend.h"
#include "assimp/scene.h"


class TestRender
{
public:
    void 初始化函数(GLFWwindow* window);

    void 渲染函数();

    void 清理函数();

private:
    const uint32_t MAX_FRAME_DRAWS = 2;
    const uint32_t MATERIAL_DESCRIPTOR_HEADROOM = 64; // 描述符池给「运行时新加模型」额外预留的材质套数
    struct  Vertex
    {
        glm::vec3 position;    // 位置
        glm::vec3 normals;     // 法线
        glm::vec4 tangents;    // 法线
        glm::vec4 color;       // 顶点色（Assimp 的 COLOR0；模型没有就填 MODEL_BASE_COLOR）
        glm::vec2 uv0;         // UV 通道 0（大多数模型只有这一套）
        glm::vec2 uv1;         // UV 通道 1（贴图 / 光照贴图等，没有就是 0）
        glm::vec2 uv2;         // UV 通道 2
        glm::vec2 uv3;         // UV 通道 3
        uint32_t meshIndex;    // 来源网格（mesh）序号，传给片元着色器
        uint32_t vertexId;     // 顶点 ID：该顶点在整个顶点缓冲里的下标
    };
    //结构体-家族队列
    struct QueueFamilyIndices//在查找物理设备时使用
    {
        uint32_t graphicsFamily = UINT32_MAX; // 图形队列家族索引（UINT32_MAX = 还没找到）
        uint32_t presentFamily = UINT32_MAX; // 呈现队列家族索引（UINT32_MAX = 还没找到）
        bool isComplete() const { return graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX; } // 两个家族都找到了才算完整
    };
    //结构体-交换链
    struct Swapchain
    {
        VkSwapchainKHR handle = VK_NULL_HANDLE;      // 交换链
        VkFormat format = VK_FORMAT_UNDEFINED;       // 交换链图像格式
        VkExtent2D extent = {};                      // 交换链图像尺寸（宽 × 高）
        std::vector<VkImage> images;                 // 交换链图像
        std::vector<VkImageView> imageViews;         // 交换链图像视图
        std::vector<VkFramebuffer> framebuffers;     // 帧缓冲（每张图像一个）
    };
    //结构体-GPU图像
    struct GPUImage
    {
        VkImage image = VK_NULL_HANDLE;             // 图像句柄
        VkDeviceMemory memory = VK_NULL_HANDLE;     // 图像占用的显存
        VkImageView view = VK_NULL_HANDLE;          // 图像视图（帧缓冲 / 采样器实际用的是它）
    };
    //结构体-GPU缓冲
    struct GpuBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;           // 缓冲句柄
        VkDeviceMemory memory = VK_NULL_HANDLE;     // 缓冲占用的显存
    };
    //结构体-同步信号量
    struct FrameResources
    {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE; // 本帧的命令缓冲（每帧重录）
        GpuBuffer uniformBuffer;                        // 相机统一缓冲（每帧写 view/projection）
        void* uniformBufferMapped = nullptr;            // 统一缓冲映射指针
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE; // 本帧的相机描述符集
        VkSemaphore imageAvailable = VK_NULL_HANDLE;    // 图像可用信号量
        VkFence inFlight = VK_NULL_HANDLE;              // 在途帧栅栏（保证命令缓冲可安全重录）
    };
    //结构体-
    struct UniformBuffer
    {
        glm::vec4 _Time;                    // 时间（模拟Unity）
        glm::vec4 _MainLightColor;          //主光源参数
        glm::vec4 _MainLightDirection;      //主光源参数
        glm::vec4 _WorldSpaceCameraPos;     // 相机位置16字节
        glm::mat4 _MATRIX_V;                // 视图矩阵（世界 → 相机）
        glm::mat4 _MATRIX_P;                // 投影矩阵（相机 → 裁剪空间）
    };
private:
    // 所有 Vulkan 句柄都必须显式初始化成 VK_NULL_HANDLE：
    // TestRender 是在栈上默认初始化的，裸句柄成员会残留随机值，
    // 那些用"句柄是否为 null"做判断的地方（比如渲染里的 pipelineLayout）就会拿着垃圾值去调 Vulkan，直接访问违规。
    GLFWwindow* window = nullptr;
    //创建实例_1
    VkInstance instance = VK_NULL_HANDLE;
    //创建表面_2
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    //选物理设备_3
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    //创建逻辑设备_4
    QueueFamilyIndices indices;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics = VK_NULL_HANDLE;
    VkQueue present = VK_NULL_HANDLE;
    //创建命令池_5
    VkCommandPool commandPool = VK_NULL_HANDLE;
    //创建交换链_6
    Swapchain swapchain;
    //创建渲染通道_7
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    //创建描述符集布局_8
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    //加载着色器模块_9
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;
    //创建贴图采样器_10
    VkSampler textureSampler = VK_NULL_HANDLE;
    //创建图形管线_11
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    //创建MSAA颜色附件_12 & 创建深度附件_13
    GPUImage msaaImage;
    GPUImage depthImage;
    //创建描述符池
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    //相机描述符集（每个在途帧一个，所有 SubMesh 共用同一份 view/projection）
    std::vector<VkDescriptorSet> cameraDescriptorSets;

    //创建同步对象
    std::vector<VkSemaphore> renderFinishedSemaphores;// 渲染完成信号量，每个交换链图像一个
    std::vector<FrameResources> frames;               // 在途帧资源（按 currentFrame 索引）
    uint32_t currentFrame = 0;                        // 当前在途帧下标
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2; // 同时在途的帧数

    bool 初始化完成 = false;   // 初始化全部成功才置 true；失败时渲染循环直接退出，避免拿空句柄继续跑

    //imgui
    Test_ImguiVulaknBackend GUI;


    //========================
    void 创建实例_1();          //->instance
    void 创建表面_2();          //(instance，window)->surface
    void 选物理设备_3();         //(instance,surface)->physicalDevice
    void 创建逻辑设备_4();       //(physicalDevice,surface)->uint32 * 2; VkQueue * 2;
    void 创建命令池_5();        //(device)->VkCommandPool;
    void 创建交换链_6();        //(physicalDevice,device,surface,window)-> ;
    void 创建渲染通道_7();      //(physicalDevice,device,sampleCount)->renderPass
    void 创建描述符集布局_8();   //
    void 加载着色器模块_9();     //(device)->VkShaderModule
    void 创建贴图采样器_10();    //(physicalDevice)->textureSampler
    void 创建图形管线_11();     //(vertShaderModule,fragShaderModule,extent2D,sampleCount,
                             //descriptorSetLayout,device,renderPass)->pipelineLayout,pipeline
    void 创建MSAA颜色附件_12(); //(physicalDevice,device,swapchain,sampleCount) ->msaaImage
    void 创建深度附件_13();    //(physicalDevice,device,swapchain,sampleCount) ->depthImage
    void 创建帧缓冲_14();


    void 重建交换链();


    void 上传顶点缓冲_15();
    void 上传索引缓冲_16();


    void 计算各模型变换矩阵_17();
    void 创建UniformBuffer_18();//UniformBuffer
    void 创建描述符池_19();
    void 分配相机描述符集_20();
    void 分配绘制命令缓冲_21();
    void 创建同步对象_22();


    void 初始化ImGui_23();
    //========================
    //模型渲染
    void 上传网格缓冲(Other1::Mesh& mesh);                  // 顶点 + 索引缓冲（走暂存缓冲）
    VkShaderModule 创建着色器模块(const std::vector<char>& spv);
    void 创建材质管线(Other1::Mesh& mesh);                  // 首次调用时顺带建好共享的 pipelineLayout
    void 更新相关Uniform();                                   // 用 GUI 里的相机算 view / projection 写进 UBO
    VkSampleCountFlagBits 选定采样数_7_3();                    // 挑一个颜色 / 深度都支持的 MSAA 采样数
    //========================
    QueueFamilyIndices 查找物理设备_家族队列_3_1(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool 查找物理设备_扩展_3_2(VkPhysicalDevice device);
    bool 查找物理设备_表面和呈现_3_3(VkPhysicalDevice device, VkSurfaceKHR surface);
    //========================
    VkSurfaceFormatKHR 选定表面格式6_1(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkPresentModeKHR 选定呈现模式6_2(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkExtent2D 选定交换链分辨率6_3(GLFWwindow* window,VkSurfaceCapabilitiesKHR capabilities);
    //========================
    VkFormat 选定深度格式7_1(VkPhysicalDevice device);
    bool 选定深度模板分量_7_2(VkFormat format);
    //========================
    VkShaderModule 读取着色器文件(VkDevice device,const std::string& filename);
    void 创建VkImage_12(VkPhysicalDevice physicalDevice,VkDevice device,uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                    VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits samples,
                    VkImage& image, VkDeviceMemory& imageMemory);
    void 创建图像视图_12(VkDevice device,VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,VkImageView& imageView);
    uint32_t 选显存类型_12(VkPhysicalDevice physicalDevice,uint32_t typeFilter, VkMemoryPropertyFlags properties);
    //========================
    void 创建Buffer(VkDevice device,VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                    VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void 复制Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void 创建临时命令缓冲(VkDevice device,VkCommandPool commandPool,VkCommandBuffer& commandBuffer);
    void 结束临时命令缓冲(VkDevice device,VkCommandPool commandPool,VkCommandBuffer commandBuffer,VkQueue graphics);
};


#endif //VULKAN_CPP_LEARNING_TESTRENDER_H
