//
// Created by LiMi on 2026/9/14.
//

#include "VulkanRenderer.h"
#include <array>
#include <limits>

VulkanRenderer::VulkanRenderer()
{
}

int VulkanRenderer::init(GLFWwindow* window)
{
    this->window = window;

    try
    {
        createInstance_1();         //执行实例化Vulkan函数
        createDebugCallback_2();    //
        createSurface_3();          //创建表面
        getPhysicalDevice_4();      //获取物理设备
        createLogicalDevice_5();    //创建逻辑设备
        createSwapChain_6();        //创建交换链
        createRenderPass_7();         //创建Pass
        createGraphicsPipeline_8();   //创建图形管线
    }
    catch (const std::runtime_error& e)
    {
        printf("ERROR==: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::cleanup()
{
    vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline,nullptr);
    vkDestroyPipelineLayout(mainDevice.logicalDevice,pipelineLayout, nullptr);
    vkDestroyRenderPass(mainDevice.logicalDevice,renderPass,nullptr);
    for (auto image :swapChainImages)
    {
        vkDestroyImageView(mainDevice.logicalDevice,image.imageView,nullptr);
    }



    vkDestroySwapchainKHR(mainDevice.logicalDevice,swapchain,nullptr);  //清除交换链
    vkDestroySurfaceKHR(instance, surface, nullptr);                    //清楚表面
    vkDestroyDevice(mainDevice.logicalDevice, nullptr);                 //清除逻辑设备
    /*if (validationEnabled) {
        DestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);
    }*/
    vkDestroyInstance(instance, nullptr);                               //清除实例
}

VulkanRenderer::~VulkanRenderer()
{
}

//1-创建实例
void VulkanRenderer::createInstance_1()
{
    //关于应用程序本身的说明
    //此处的大部分数据不影响程序运行，仅用于开发人员方便。
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Renderer"; //应用程序名称
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); //应用程序版本号
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); //引擎版本号
    appInfo.pEngineName = "No Engine"; //引擎名称
    appInfo.apiVersion = VK_API_VERSION_1_4; //期望使用的 Vulkan API 版本


    //Vkinstance的创建信息(Vulkan实例）
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = 0; //VK_WHATEVER | VK_OTHERTHING;
    createInfo.pApplicationInfo = &appInfo;


    //创建一个列表来存储实例扩展
    std::vector<const char*> instanceExtensions = std::vector<const char*>();

    //设置扩展实例将使用
    uint32_t glfwExtensionCount = 0; //GLFW可能需要多个扩展
    const char** glfwExtensions; //扩展作为字符串数组传递，因此需要指针（该数组）指向指针（该字符串）

    //获取GLFW扩展
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    //将GLFW扩展添加到Vkinstance扩展列表中
    for (size_t i = 0; i < glfwExtensionCount; i++)
    {
        instanceExtensions.push_back(glfwExtensions[i]);
    }

    //检查实例扩展支持
    if (!checkInstanceExtensionSupport_1_(&instanceExtensions))
    {
        //内部函数
        throw std::runtime_error("VkInstance does not support required extensions!");
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    //待办：设置实例将使用的验证层
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    //创建实例
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}

//2-
void VulkanRenderer::createDebugCallback_2()
{
}

//3-创建表面
void VulkanRenderer::createSurface_3()
{
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create window surface!");
    }
}

//5-创建逻辑设备
void VulkanRenderer::createLogicalDevice_5()
{
    //获取所选物理设备的队列家族索引
    QueueFamilyIndices_u indices = getQueueFamilies_56A_(mainDevice.physicalDevice);

    // 用于队列创建信息的向量，以及用于家族索引的设置
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilyIndices = {indices.graphicsFamily, indices.presentFamily};

    //队列逻辑设备需要创建的信息（目前仅支持一个，后续将增加更多！）
    for (int queueFamily : queueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily; //用于创建队列的家族索引
        queueCreateInfo.queueCount = 1; //要创建的队列数量
        float priority = 1.0f;
        queueCreateInfo.pQueuePriorities = &priority; //Vulkan需要知道如何处理多个队列

        queueCreateInfos.push_back(queueCreateInfo);
    }
    //逻辑设备将使用的物理设备特性
    VkPhysicalDeviceFeatures deviceFeatures = {}; //物理设备功能逻辑设备将使用

    //创建逻辑设备的信息（有时称为“设备”）
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());  //队列创建信息数量
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();                            //队列创建信息列表，以便设备可以创建所需的队列
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); //已启用的逻辑设备扩展数量
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();                      //已启用的逻辑设备扩展列表
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    //为给定的物理设备创建逻辑设备
    VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device!");
    }
    //队列与设备同时创建  因此我们希望处理队列。
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentFamily, 0, &presentationQueue);
}

//6-创建交换链
void VulkanRenderer::createSwapChain_6()
{
    //获取交换链详细信息，以便我们选择最佳设置
    SwapChainDetails_u swapChainDetails = getSwapChainDetails_A6(mainDevice.physicalDevice);

    //为我们的交换链找到最佳表面值
    VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
    VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

    //交换链中有多少张图像？请至少获取一个，以实现三缓冲。
    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

    // If imageCount higher than max, then clamp down to max
    if (swapChainDetails.surfaceCapabilities.maxImageCount &&
        swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
    {
        imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
    }

    //交换链的创建信息
    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface;                              //交换链表面
    swapChainCreateInfo.imageFormat = surfaceFormat.format;             //交换链格式
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;     //交换链色彩空间
    swapChainCreateInfo.presentMode = presentMode;                      //交换链显示模式
    swapChainCreateInfo.imageExtent = extent;                           //交换链图像范围
    swapChainCreateInfo.minImageCount = imageCount;                     //交换链中最小图像数量
    swapChainCreateInfo.imageArrayLayers = 1;                           //Number of layers for each image in chain
    swapChainCreateInfo.imageUsage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//What attachment images will be used as
    swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;//Transform to perform on swap chain images
    swapChainCreateInfo.compositeAlpha =VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;//How to handle blending images with external graphics (e.g. other windows)
    swapChainCreateInfo.clipped=VK_TRUE;//Whether to clip parts of image not in view (e.g. behind another window, off screen, etc)


    // 获取队列家族索引
    QueueFamilyIndices_u indices = getQueueFamilies_56A_(mainDevice.physicalDevice);
    // 如果图形和演示文稿家族不同，那么交换链必须允许图像在不同家族之间共享。
    if (indices.graphicsFamily !=indices.presentFamily)
    {
        uint32_t queueFamilyIndices[] ={
            (uint32_t)indices.graphicsFamily,
            (uint32_t)indices.presentFamily,
        };

        swapChainCreateInfo.imageSharingMode =VK_SHARING_MODE_CONCURRENT; // 图像共享处理
        swapChainCreateInfo.queueFamilyIndexCount =2;                     // 用于共享图像的队列数量
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;     // 用于共享的队列数组
    }
    else
    {
        swapChainCreateInfo.imageSharingMode =VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount =0;
        swapChainCreateInfo.pQueueFamilyIndices =nullptr;
    }

    // 如果旧的交换链被销毁且此链接替了它，则将旧链连接起来以快速移交责任
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    //创建交换链
    VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice,&swapChainCreateInfo,nullptr,&swapchain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Swapchain!");
    }


    // Store for later reference
    swapChainImageFormat =surfaceFormat.format;
    swapChainExtent = extent;

    //获取交换链图像（先计数，再取值）
    uint32_t swapChainImageCount;
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice,swapchain, &swapChainImageCount,nullptr);

    std::vector<VkImage>images(swapChainImageCount);
    vkGetSwapchainImagesKHR(mainDevice.logicalDevice,swapchain,&swapChainImageCount,images.data());

    for (VkImage image : images)
    {
        // Store image handle
        SwapchainImage_u swapChainImage = {};
        swapChainImage.image =image;
        swapChainImage.imageView = createImageView(image,swapChainImageFormat,VK_IMAGE_ASPECT_COLOR_BIT);

        swapChainImages.push_back(swapChainImage);
    }

}

//7-创建渲染Pass
void VulkanRenderer::createRenderPass_7()
{
    // Colour attachment of render pass
    VkAttachmentDescription colourAttachment = {};
    colourAttachment.format =swapChainImageFormat;                      // 用于附件的格式
    colourAttachment.samples =VK_SAMPLE_COUNT_1_BIT;                    // 多采样时要写入的样本数量
    colourAttachment.loadOp =VK_ATTACHMENT_LOAD_OP_CLEAR;               // 描述渲染前对附件的操作
    colourAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;            // 描述渲染后对附件的操作
    colourAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // 描述渲染前对描边的处理
    colourAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 描述渲染后对描边的处理

    // 帧缓冲区数据将以图像形式存储，但图像可以采用不同的数据布局，以实现特定操作的最优利用。
    colourAttachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;           // Image data layout before render pass starts
    colourAttachment.finalLayout =VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // Image data layout after render pass (to change to)


    // 附件引用使用一个附件索引，该索引指向传递给 renderPassCreateInfo 的附件列表中的索引。
    VkAttachmentReference colourAttachmentReference ={};
    colourAttachmentReference.attachment =0;
    colourAttachmentReference.layout =VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 关于渲染通道所使用的特定子通道的信息
    VkSubpassDescription subpass ={};
    subpass.pipelineBindPoint =VK_PIPELINE_BIND_POINT_GRAPHICS;// Pipeline type subpass is to be bound to
    subpass.colorAttachmentCount =1;
    subpass.pColorAttachments = &colourAttachmentReference;

    // 需要通过子通道依赖来确定布局过渡发生的时间
    std::array<VkSubpassDependency,2> subpassDependencies;

    // 从 VK_IMAGE_LAYOUT_UNDEFINED 转换为 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;
    // 从 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 到 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 的转换
    // 转换必须在……之后进行
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;;
    // But must happen before...
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType =VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colourAttachment;
    renderPassCreateInfo.subpassCount =1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount =static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo,nullptr,&renderPass);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Render Pass!");
    }

}

//8-创建图形管线
void VulkanRenderer::createGraphicsPipeline_8()
{
    // 读取着色器的SPIR-V代码
    auto vertexShaderCode = readFile_u("shaders/vert.spv");
    auto fragmentShaderCode = readFile_u("shaders/frag.spv");

    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);


    //--SHADER STAGE CREATION INFORMATION -
    // 顶点阶段创建信息
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
    vertexShaderCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderCreateInfo.stage =VK_SHADER_STAGE_VERTEX_BIT;       // 着色器阶段名称
    vertexShaderCreateInfo.module = vertexShaderModule;             // 该阶段将使用的着色器模块
    vertexShaderCreateInfo.pName ="main";                           // 着色器的入口点
    // 片段阶段创建信息
    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo ={};
    fragmentShaderCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // 着色器阶段名称
    fragmentShaderCreateInfo.module =fragmentShaderModule;          // 该阶段将使用的着色器模块
    fragmentShaderCreateInfo.pName ="main";                         // 着色器的入口点

    // 将着色器阶段创建信息放入数组
    // 图形管线创建信息需要一个着色器阶段创建的数组
    VkPipelineShaderStageCreateInfo shaderStages[]={vertexShaderCreateInfo,fragmentShaderCreateInfo};


    // -- VERTEX INPUT (TODo：在创建资源时添加顶点描述)-
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount =0;
    vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;// 顶点绑定描述列表
    vertexInputCreateInfo.vertexAttributeDescriptionCount =0;
    vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;// 顶点属性描述列表

    // -- INPUT ASSEMBLY --
    VkPipelineInputAssemblyStateCreateInfo inputAssembly ={};
    inputAssembly.sType =VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology =VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;    // 将原始类型用于组装顶点
    inputAssembly.primitiveRestartEnable =VK_FALSE;                 // 允许覆盖“条带”拓扑以开始新的原始类型


    //-- VIEWPORT & SCISSOR
    // 创建视口信息结构
    VkViewport viewport={};
    viewport.x= 0.0f;                               // x 起始坐标
    viewport.y =0.0f;                               // y 起始坐标
    viewport.width=(float)swapChainExtent.width;    // 视口宽度
    viewport.height =(float)swapChainExtent.height; // 视口高度
    viewport.minDepth =0.0f;                        // 最小帧缓冲区深度
    viewport.maxDepth =1.0f;                        // 最大帧缓冲区深度


    // 创建一个剪刀信息结构体
    VkRect2D scissor ={};
    scissor.offset = {0,0};                 // 偏移以使用区域
    scissor.extent = swapChainExtent;                // 指定要使用的区域范围，从偏移量开始

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo ={};
    viewportStateCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount =1;
    viewportStateCreateInfo.pViewports =&viewport;
    viewportStateCreateInfo.scissorCount =1;
    viewportStateCreateInfo.pScissors =&scissor;

    /*// -- DYNAMIC STATES -
    // 动态状态以启用
    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);// 动态视口：可通过 vkCmdSetViewport(commandbuffer, e, 1, &viewport) 在命令缓冲区中调整大小；
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);// 动态剪切区域：可通过 vkCmdSetScissor(commandbuffer, o, 1, &scissor) 在命令缓冲区中调整大小。

    // 动态状态创建信息
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
    dynamicStateCreateInfo.pDynamicStates =dynamicStateEnables.data();*/


    // -- RASTERIZER -
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable=VK_FALSE;
    rasterizerCreateInfo.rasterizerDiscardEnable =VK_FALSE;
    rasterizerCreateInfo.polygonMode =VK_POLYGON_MODE_FILL;
    rasterizerCreateInfo.lineWidth =1.0f;
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizerCreateInfo.frontFace =VK_FRONT_FACE_CLOCKWISE;
    rasterizerCreateInfo.depthBiasEnable =VK_FALSE;


    // -- MULTISAMPLING --
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
    multisamplingCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable=VK_FALSE;// Enable multisample shading or not
    multisamplingCreateInfo.rasterizationSamples =VK_SAMPLE_COUNT_1_BIT;// Number of samples to use per fragment


    // Blend Attachment State (how blending is handled)
    VkPipelineColorBlendAttachmentState colourState = {};
    colourState.colorWriteMask =VK_COLOR_COMPONENT_R_BIT |VK_COLOR_COMPONENT_G_BIT// Colours to apply blending to
    |VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    colourState.blendEnable =VK_TRUE;// Enable blending

    // Blending uses equation:(srcColorBlendFactor * new colour) colorBlendOp (dstColorBlendFactor * old colour)
    colourState.srcColorBlendFactor =VK_BLEND_FACTOR_SRC_ALPHA;
    colourState.dstColorBlendFactor =VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colourState.colorBlendOp=VK_BLEND_OP_ADD;

    colourState.srcAlphaBlendFactor =VK_BLEND_FACTOR_ONE;
    colourState.dstAlphaBlendFactor =VK_BLEND_FACTOR_ZERO;
    colourState.alphaBlendOp=VK_BLEND_OP_ADD;



    VkPipelineColorBlendStateCreateInfo colourBlendingCreateInfo = {};
    colourBlendingCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colourBlendingCreateInfo.logicOpEnable =VK_FALSE;
    colourBlendingCreateInfo.attachmentCount =1;
    colourBlendingCreateInfo.pAttachments = &colourState;

    // Summarised:(1 *new alpha) + (e* old alpha)= new alpha
    //
    //-- PIPELINE LAYOUT (TODO:Apply Future Descriptor Set Layouts) --
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo ={};
    pipelineLayoutCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount =0;
    pipelineLayoutCreateInfo.pSetLayouts =nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount =0;
    pipelineLayoutCreateInfo.pPushConstantRanges =nullptr;


    VkResult result =vkCreatePipelineLayout(mainDevice.logicalDevice,&pipelineLayoutCreateInfo,nullptr,&pipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Pipeline Layout!");
    }




    // -- GRAPHICSPIPELINE CREATION --
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages =shaderStages;
    pipelineCreateInfo.pVertexInputState =&vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState =&viewportStateCreateInfo;
    pipelineCreateInfo.pDynamicState =nullptr;
    pipelineCreateInfo.pRasterizationState =&rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState =&multisamplingCreateInfo;
    pipelineCreateInfo.pColorBlendState =&colourBlendingCreateInfo;
    pipelineCreateInfo.pDepthStencilState =nullptr;
    pipelineCreateInfo.layout =pipelineLayout;
    pipelineCreateInfo.renderPass =renderPass;
    pipelineCreateInfo.subpass = 0;

    pipelineCreateInfo.basePipelineHandle =VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex =-1;


    // Create Graphics Pipeline
    result =vkCreateGraphicsPipelines(mainDevice.logicalDevice,VK_NULL_HANDLE,1,&pipelineCreateInfo,nullptr,&graphicsPipeline);
    if (result !=VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Graphics Pipeline!");
    }

    // CREATE PIPELINE
    // 销毁着色器模块，管道创建后不再需要
    vkDestroyShaderModule(mainDevice.logicalDevice,fragmentShaderModule,nullptr);
    vkDestroyShaderModule(mainDevice.logicalDevice,vertexShaderModule,nullptr);
}


//2-获取物理设备
void VulkanRenderer::getPhysicalDevice_4()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("No Vulkan instance");
    }
    std::vector<VkPhysicalDevice> devicesList(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devicesList.data());


    for (const auto& device : devicesList)
    {
        if (checkDeviceSuitable_4_A(device))
        {
            mainDevice.physicalDevice = device;
            break;
        }
    }
}

//检查实例拓展
bool VulkanRenderer::checkInstanceExtensionSupport_1_(std::vector<const char*>* checkExtensions)
{
    //需要获取扩展数量以创建正确大小的数组来存储扩展。
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    //使用count创建VkExtensionProperties列表
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    //检查给定的扩展是否在可用扩展列表中
    bool hasExtension = false;
    for (const auto& checkExtension : *checkExtensions)
    {
        for (const auto& extension : extensions)
        {
            if (strcmp(checkExtension, extension.extensionName))
            {
                hasExtension = true;
                break;
            }
        }
        if (!hasExtension)
        {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport_A_(VkPhysicalDevice device)
{
    //获取设备扩展数量
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    //如果未找到扩展，返回失败
    if (extensionCount == 0)
    {
        return false;
    }
    //填充扩展列表
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    //检查扩展程序
    for (const auto& deviceExtension : deviceExtensions)
    {
        bool hasExtension = false;
        for (const auto& extension : extensions)
        {
            if (strcmp(deviceExtension, extension.extensionName) == 0)
            {
                hasExtension = true;
                break;
            }
        }
        if (!hasExtension)
        {
            return false;
        }
    }
    return true;

    return true;
}

bool VulkanRenderer::checkValidationLayerSupport()
{
    return true;
}

//检查合格设备
bool VulkanRenderer::checkDeviceSuitable_4_A(VkPhysicalDevice device)
{
    /*// 关于设备本身的详细信息（ID、名称、类型、厂商等）
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // 关于设备功能的信息（如几何着色器、细分着色器、粗线等）
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);*/
    QueueFamilyIndices_u indices = getQueueFamilies_56A_(device);
    bool extensionsSupported = checkDeviceExtensionSupport_A_(device);

    bool swapChainValid = false;
    if (extensionsSupported)
    {
        SwapChainDetails_u swapChainDetails = getSwapChainDetails_A6(device);
        swapChainValid = !swapChainDetails.presentModes.empty() && !swapChainDetails.formats.empty();
    }


    return indices.isVlid() && extensionsSupported && swapChainValid;
}

//获取队列家族 （检查合格设备，）
QueueFamilyIndices_u VulkanRenderer::getQueueFamilies_56A_(VkPhysicalDevice device)
{
    QueueFamilyIndices_u indices;

    //获取指定设备的所有队列家族属性信息
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

    //遍历每个队列家族，检查其是否至少包含所需类型的队列之一
    int i = 0;
    for (const auto& queueFamily : queueFamilyList)
    {
        //首先检查队列家族是否至少包含一个队列（可能没有队列)
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i; // 如果队列家族有效，则获取索引
        }
        //检查队列家族是否支持呈现
        VkBool32 presentFamily = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentFamily);
        //检查队列是否为演示类型【（可以是图形和演示的组合）
        if (queueFamily.queueCount > 0 && presentFamily)
        {
            indices.presentFamily = i;
        }
        //检查队列家族索引是否处于有效状态，如果是则停止搜索
        if (indices.isVlid())
        {
            mainDevice.physicalDevice = device;
            break;
        }
        i++;
    }
    return indices;
}

//获取交换链
SwapChainDetails_u VulkanRenderer::getSwapChainDetails_A6(VkPhysicalDevice device)
{
    SwapChainDetails_u swapChainDetails;
    // -- CAPABILITIES--
    // 获取指定物理设备上给定表面的表面特性
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

    // -- FORMATS -
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        swapChainDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
    }

    // -- PRESENTATION MODES --
    uint32_t presentationCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);
    //如果返回了演示模式，请获取演示模式列表
    if (presentationCount != 0)
    {
        swapChainDetails.presentModes.resize(presentationCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount,
                                                  swapChainDetails.presentModes.data());
    }


    return swapChainDetails;
}

//最佳格式是主观的，但我们的将是：格式 VKFORMAT_R8G8B8A8UNORM
//Format  VK_FORMAT_R8G8B8A8_UNORM
//colorSpace  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
    // 如果仅有一种格式可用且未定义，则表示所有格式均可用（无限制）
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    };
    // 如果受限，请搜索最佳格式
    for (const auto& format : formats)
    {
        if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    //如果找不到最优格式，则直接返回第一个格式
    return formats[0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(
    const std::vector<VkPresentModeKHR> presentationModes)
{
    //查找邮箱演示模式
    for (const auto& presentationMode : presentationModes)
    {
        if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return presentationMode;
        }
    }

    //如果找不到，就使用FIFO，因为Vulkan规范要求必须如此
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    //如果当前范围位于数值限制处，则范围可以变化。否则，它就是窗口的大小。
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        //使用窗口大小创建新范围
        VkExtent2D actualExtent = {};
        actualExtent.width = static_cast<uint32_t>(width);
        actualExtent.height = static_cast<uint32_t>(height);
        //表面还定义了最大值和最小值，因此通过限制值确保在边界内。
        actualExtent.width = std::max(capabilities.minImageExtent.width,
                                      std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height,
                                       std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewCreateInfo ={};
    viewCreateInfo.sType =VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image =image;                                    // 用于创建视图的图像类型
    viewCreateInfo.viewType =VK_IMAGE_VIEW_TYPE_2D;                 // 图像类型（1D、2D、3D、立方体等）
    viewCreateInfo.format =format;                                  // 图像数据格式
    viewCreateInfo.components.r=VK_COMPONENT_SWIZZLE_IDENTITY;      // 是否允许将 rgba 组件重新映射为其他 rgba 值
    viewCreateInfo.components.g=VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b=VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a=VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.subresourceRange.aspectMask = aspectFlags;       // 要查看图像的哪个方面（例如，CoLoR_BIT 用于查看颜色）




    // ubresources 允许视图仅显示图像的一部分
    viewCreateInfo.subresourceRange.aspectMask = aspectFlags;       // 要查看图像的哪个方面
    viewCreateInfo.subresourceRange.baseMipLevel =0;                // 从哪个mipmap级别开始查看
    viewCreateInfo.subresourceRange.levelCount =1;                  // 要查看的mipmap级别数量
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;             // 从哪个数组级别开始查看
    viewCreateInfo.subresourceRange.layerCount =1;                  // 要查看的数组级别数量

    // 创建图像视图并返回
    VkImageView imageView;
    VkResult result =vkCreateImageView(mainDevice.logicalDevice,&viewCreateInfo,nullptr,&imageView);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an Image View!");
    }

    return imageView;

}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
    // Shader Module creation information
    VkShaderModuleCreateInfo shaderModuleCreateInfo={};
    shaderModuleCreateInfo.sType =VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize =code.size();
    // Size of code
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
    // Pointer to code (of uint32_t pointer type)
    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(mainDevice.logicalDevice,&shaderModuleCreateInfo,nullptr,&shaderModule);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a shader module!");
    }
    return shaderModule;
}
