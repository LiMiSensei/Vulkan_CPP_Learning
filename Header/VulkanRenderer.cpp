//
// Created by LiMi on 2026/9/14.
//

#include "VulkanRenderer.h"
#include <array>
#include <limits>

#include "Mesh.h"
#include "VulkanValidation.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

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
        createRenderPass_7();       //创建Pass

        createDescriptorSetLayout_8();

        createGraphicsPipeline_9(); //创建图形管线

        createFramebuffers_10();        //帧缓冲
        createCommandPool_11();        //命令池

        //=================================================================
        ubo_VP.projection = glm::perspective(glm::radians(45.0f), (float)swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 100.0f);
        ubo_VP.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ubo_VP.projection[1][1] *= -1;

        std::vector<Vertex_u> meshVertices = {
            { { -0.45, -0.4, 0.0 },{ 1.0f, 0.0f, 0.0f } },	// 0
            { { -0.1, 0.4, 0.0 },{ 0.0f, 1.0f, 0.0f } },	    // 1
            { { -0.9, 0.4, 0.0 },{ 0.0f, 0.0f, 1.0f } },    // 2
            //{ { -0.9, -0.4, 0.0 },{ 1.0f, 1.0f, 0.0f } },   // 3
        };

        std::vector<Vertex_u> meshVertices2 = {
            { { 0.9, -0.3, 0.0 },{ 1.0f, 0.0f, 0.0f } },	  // 0
            { { 0.9, 0.3, 0.0 },{ 0.0f, 1.0f, 0.0f } },	  // 1
            { { 0.1, 0.3, 0.0 },{ 0.0f, 0.0f, 1.0f } },    // 2
            { { 0.1, -0.3, 0.0 },{ 1.0f, 1.0f, 0.0f } },   // 3
        };

        // Index Data
        std::vector<uint32_t> meshIndices = {
            0, 1, 2,
            2, 3, 0
        };

        Mesh firstMesh = Mesh(physicalDevice, logicalDevice,
            graphicsQueue, graphicsCommandPool,
            &meshVertices, &meshIndices);
        Mesh secondMesh = Mesh(physicalDevice, logicalDevice,
            graphicsQueue, graphicsCommandPool,
            &meshVertices2, &meshIndices);

        meshList.push_back(firstMesh);
        meshList.push_back(secondMesh);
        //=================================================================
        createCommandBuffers_12();     //命令缓冲区
        //=================================================================

        allocateDynamicBufferTransferSpace_13();
        createUniformBuffers_14();
        createDescriptorPool_15();
        createDescriptorSets_16();

        //=================================================================
        createSynchronisation_18();    //信号量和栅栏

        //=================================================================
        initImGui();                   //初始化 ImGui（需要用到上面创建好的 Vulkan 资源）
        // 命令缓冲区在每帧 draw() 时才会录制（因为要写入每帧都会变化的 ImGui 绘制数据）
    }
    catch (const std::runtime_error& e)
    {
        printf("ERROR: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::draw()
{
    // -- 构建本帧界面 --
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawImGui();        // 界面内容
    ImGui::Render();    // 生成绘制数据，稍后由 recordCommands_17 录制进命令缓冲区

    // -- 获取下一张图像 --
    // 等待上一次绘制后给定的围栏信号（打开）再继续
	vkWaitForFences(logicalDevice, 1, &drawFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    //手动重置（关闭）围栏
	vkResetFences(logicalDevice, 1, &drawFences[currentFrame]);

	// 获取下一张要绘制图像的索引，并在准备好绘制时发送信号量
	uint32_t imageIndex;
	vkAcquireNextImageKHR(logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable[currentFrame], VK_NULL_HANDLE, &imageIndex);

    updateUniformBuffers(imageIndex);

	// -- 重新录制命令缓冲区 --
	// ImGui 的绘制数据每帧都会变化，所以命令缓冲区必须每帧重新录制。
	// 如果上一次使用该命令缓冲区的帧还没有执行完，就先等待它对应的栅栏，避免重录正在执行的命令缓冲区。
	if (commandBufferFences[imageIndex] != VK_NULL_HANDLE &&
		commandBufferFences[imageIndex] != drawFences[currentFrame])
	{
		vkWaitForFences(logicalDevice, 1, &commandBufferFences[imageIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	}
	recordCommands_17(imageIndex);
	// 记录该命令缓冲区本次提交所使用的栅栏，下次重录前要等它
	commandBufferFences[imageIndex] = drawFences[currentFrame];

	// -- 将命令缓冲区提交以进行渲染 --
	// 队列提交信息
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;										// 等待的信号量数量
	submitInfo.pWaitSemaphores = &imageAvailable[currentFrame];				// 等待的信号量列表
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;						        // 检查信号量的阶段
	submitInfo.commandBufferCount = 1;								        // 提交的命令缓冲区数量
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];		        // 要提交的命令缓冲区
	submitInfo.signalSemaphoreCount = 1;							        // 要发出信号的信号量数量
	submitInfo.pSignalSemaphores = &renderFinished[currentFrame];	        // 命令缓冲区完成时要发出信号的信号量

	// 将命令缓冲区提交到队列
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences[currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit Command Buffer to Queue!");
	}


	//-- 将当前图像显示在屏幕上 --
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;										// 等待的信号量数量
	presentInfo.pWaitSemaphores = &renderFinished[currentFrame];			// 要等待的信号量
	presentInfo.swapchainCount = 1;											// 需要呈现的交换链数量
	presentInfo.pSwapchains = &swapchain;									// 用于呈现图像的交换链
	presentInfo.pImageIndices = &imageIndex;								// 在交换链中呈现图像的索引

	// 当前图片
	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present Image!");
	}

	// 获取下一帧（使用 % MAX_FRAME_DRAWS 以确保值低于 MAX_FRAME_DRAWS）
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}
void VulkanRenderer::cleanup()
{
    // 在销毁前，请等待设备上没有运行任何操作。
    vkDeviceWaitIdle(logicalDevice);
    destroyImGui();
    _aligned_free(modelTransferSpace);

    vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        vkDestroyBuffer(logicalDevice, vpUniformBuffer[i], nullptr);
        vkFreeMemory(logicalDevice, vpUniformBufferMemory[i], nullptr);
        vkDestroyBuffer(logicalDevice, modelDUniformBuffer[i], nullptr);
        vkFreeMemory(logicalDevice, modelDUniformBufferMemory[i], nullptr);
    }
    for (size_t i = 0; i < meshList.size(); i++)
    {
        meshList[i].destroyBuffers();
    }
    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        vkDestroySemaphore(logicalDevice, renderFinished[i], nullptr);
        vkDestroySemaphore(logicalDevice, imageAvailable[i], nullptr);
        vkDestroyFence(logicalDevice, drawFences[i], nullptr);
    }
    vkDestroyCommandPool(logicalDevice, graphicsCommandPool, nullptr);
    for (auto framebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }
    vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
    for (auto image : swapChainImages)
    {
        vkDestroyImageView(logicalDevice, image.imageView, nullptr);
    }
    vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    if (validationEnabled)
    {
        DestroyDebugReportCallbackEXT(instance, callback, nullptr);
    }
    vkDestroyInstance(instance, nullptr);
}

//========= imgui ============
void VulkanRenderer::initImGui()
{
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // 平台后端（GLFW）：因为用 Vulkan 渲染，所以必须用 ForVulkan
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // 渲染后端（Vulkan）：直接使用渲染器已经创建好的 Vulkan 资源
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = logicalDevice;
    initInfo.QueueFamily = static_cast<uint32_t>(getQueueFamilies_56A_(physicalDevice).graphicsFamily);
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;    // 让后端自己创建描述符池
    initInfo.MinImageCount = 2;                                                        // 至少为 2
    initInfo.ImageCount = static_cast<uint32_t>(swapChainImages.size());
    initInfo.PipelineInfoMain.RenderPass = renderPass;                                  // 与渲染器共用同一个渲染通道
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        throw std::runtime_error("Failed to initialize ImGui Vulkan backend!");
    }
}
//关闭并销毁 ImGui（调用前需保证设备空闲）
void VulkanRenderer::destroyImGui()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
//界面内容
void VulkanRenderer::drawImGui()
{
    ImGuiIO& io = ImGui::GetIO();

    // 鼠标信息调试窗口
    ImGui::Begin("Mouse Info");
    ImGui::Text("Position: (%.1f, %.1f)", io.MousePos.x, io.MousePos.y);
    ImGui::Text("Left Down: %s", io.MouseDown[0] ? "true" : "false");
    ImGui::Text("Left Delta: (%.2f, %.2f)", io.MouseDelta.x, io.MouseDelta.y);
    ImGui::Text("Wheel: %.1f  WheelH: %.1f", io.MouseWheel, io.MouseWheelH);
    ImGui::End();
}
//===========================
void VulkanRenderer::updateModel(int modelId, glm::mat4 newModel)
{
    if (modelId >= meshList.size()) return;
    meshList[modelId].setModel(newModel);
}

VulkanRenderer::~VulkanRenderer()
{
}
//====================================================================================================
//1-创建实例
void VulkanRenderer::createInstance_1()
{
    if (validationEnabled && !checkValidationLayerSupport())
    {
        throw std::runtime_error("Required Validation Layers not supported!");
    }
    //关于应用程序本身的说明
    //此处的大部分数据不影响程序运行，仅用于开发人员方便。
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Renderer"; //应用程序名称
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); //应用程序版本号
    appInfo.pEngineName = "No Engine"; //引擎名称
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0); //引擎版本号
    appInfo.apiVersion = VK_API_VERSION_1_4; //期望使用的 Vulkan API 版本

    //Vkinstance的创建信息(Vulkan实例）
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    //createInfo.flags = 0; //VK_WHATEVER | VK_OTHERTHING;
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

    if (validationEnabled)
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
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
    if (validationEnabled)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }


    //创建实例
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}
//2-调试
void VulkanRenderer::createDebugCallback_2()
{
    // Only create callback if validation enabled
    if (!validationEnabled) return;

    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
    callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;	// Which validation reports should initiate callback
    callbackCreateInfo.pfnCallback = debugCallback;												// Pointer to callback function itself

    // Create debug callback with custom create function
    VkResult result = CreateDebugReportCallbackEXT(instance, &callbackCreateInfo, nullptr, &callback);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Debug Callback!");
    }
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
//4-获取物理设备
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
            physicalDevice = device;
            break;
        }
    }

    // Get properties of our new device
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}
//5-创建逻辑设备
void VulkanRenderer::createLogicalDevice_5()
{
    //获取所选物理设备的队列家族索引
    QueueFamilyIndices_u indices = getQueueFamilies_56A_(physicalDevice);

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
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions_u.size()); //已启用的逻辑设备扩展数量
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions_u.data();                      //已启用的逻辑设备扩展列表
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    //为给定的物理设备创建逻辑设备
    VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device!");
    }
    //队列与设备同时创建  因此我们希望处理队列。
    vkGetDeviceQueue(logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, indices.presentFamily, 0, &presentationQueue);
}
//6-创建交换链
void VulkanRenderer::createSwapChain_6()
{
    //获取交换链详细信息，以便我们选择最佳设置
    SwapChainDetails_u swapChainDetails = getSwapChainDetails_A6(physicalDevice);

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
    swapChainCreateInfo.surface = surface;                                                      //交换链表面
    swapChainCreateInfo.imageFormat = surfaceFormat.format;                                     //交换链格式
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;                             //交换链色彩空间
    swapChainCreateInfo.presentMode = presentMode;                                              //交换链显示模式
    swapChainCreateInfo.imageExtent = extent;                                                   //交换链图像范围
    swapChainCreateInfo.minImageCount = imageCount;                                             //交换链中最小图像数量
    swapChainCreateInfo.imageArrayLayers = 1;                                                   //链中每张图像的层数
    swapChainCreateInfo.imageUsage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;                      //将使用哪些附加图像
    swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;   //转换为在交换链图像上执行
    swapChainCreateInfo.compositeAlpha =VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;                      //如何处理将图像与外部图形（例如其他窗口）进行混合
    swapChainCreateInfo.clipped=VK_TRUE;                                                        //是否裁剪图像中未在视图范围内的部分（例如位于其他窗口后方、超出屏幕等）


    // 获取队列家族索引
    QueueFamilyIndices_u indices = getQueueFamilies_56A_(physicalDevice);
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
    VkResult result = vkCreateSwapchainKHR(logicalDevice,&swapChainCreateInfo,nullptr,&swapchain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Swapchain!");
    }


    // Store for later reference
    swapChainImageFormat =surfaceFormat.format;
    swapChainExtent = extent;

    //获取交换链图像（先计数，再取值）
    uint32_t swapChainImageCount;
    vkGetSwapchainImagesKHR(logicalDevice,swapchain, &swapChainImageCount,nullptr);
    std::vector<VkImage>images(swapChainImageCount);
    vkGetSwapchainImagesKHR(logicalDevice,swapchain,&swapChainImageCount,images.data());

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
    colourAttachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;           // 渲染通道开始前的图像数据布局
    colourAttachment.finalLayout =VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;      // 渲染通道后的图像数据布局（用于更改）


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
    subpassDependencies[1].dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType =VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colourAttachment;
    renderPassCreateInfo.subpassCount =1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount =static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(logicalDevice, &renderPassCreateInfo,nullptr,&renderPass);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Render Pass!");
    }

}
//8-创建描述符集布局
void VulkanRenderer::createDescriptorSetLayout_8()
{
    // UboViewProjection 绑定信息
    VkDescriptorSetLayoutBinding vpLayoutBinding = {};
    vpLayoutBinding.binding = 0;											// 着色器中的绑定点（通过着色器中的绑定编号指定）
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	    // 描述符类型（uniform、动态 uniform、图像采样器等）
    vpLayoutBinding.descriptorCount = 1;									// 绑定描述符的数量
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				// 要绑定的着色器阶段
    vpLayoutBinding.pImmutableSamplers = nullptr;							// 对于纹理：可通过在 layout 中指定来使采样器数据不可变（不可修改）

    // 模型绑定信息
    VkDescriptorSetLayoutBinding modelLayoutBinding = {};
    modelLayoutBinding.binding = 1;
    modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelLayoutBinding.descriptorCount = 1;
    modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    modelLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings = { vpLayoutBinding, modelLayoutBinding };

    // 使用给定的绑定创建描述符集布局
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());	//绑定信息数量
    layoutCreateInfo.pBindings = layoutBindings.data();								//绑定信息数组

    // 创建描述符集布局
    VkResult result = vkCreateDescriptorSetLayout(logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Descriptor Set Layout!");
    }
}
//9-创建图形管线
void VulkanRenderer::createGraphicsPipeline_9()
{
    // 读取着色器的SPIR-V代码
    auto vertexShaderCode = readFile_u("shaders/vert.spv");
    auto fragmentShaderCode = readFile_u("shaders/frag.spv");

    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);


    //--着色器阶段创建信息 --
    //ToDo: 顶点阶段创建信息
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
    vertexShaderCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderCreateInfo.stage =VK_SHADER_STAGE_VERTEX_BIT;       // 着色器阶段名称
    vertexShaderCreateInfo.module = vertexShaderModule;             // 该阶段将使用的着色器模块
    vertexShaderCreateInfo.pName ="main";                           // 着色器的入口点
    //ToDo: 片段阶段创建信息
    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo ={};
    fragmentShaderCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;  // 着色器阶段名称
    fragmentShaderCreateInfo.module =fragmentShaderModule;          // 该阶段将使用的着色器模块
    fragmentShaderCreateInfo.pName ="main";                         // 着色器的入口点

    //ToDo: 将着色器阶段创建信息放入数组
    // 图形管线创建信息需要一个着色器阶段创建的数组
    VkPipelineShaderStageCreateInfo shaderStages[]={vertexShaderCreateInfo,fragmentShaderCreateInfo};

    //============================================================================================
    //单个顶点的颜色、纹理坐标、法线等数据如何整体处理
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding =0;
    bindingDescription.stride = sizeof(Vertex_u);
    bindingDescription.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;

    // 否则越界写会破坏相邻栈变量（例如 bindingDescription）

    std::array<VkVertexInputAttributeDescription,2> attributeDescriptions;

    // Position Attribute
    attributeDescriptions[0].binding = 0;                           // 数据绑定的位置（应与上述相同）
    attributeDescriptions[0].location = 0;                          // 着色器中从何处读取数据的位置
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;;  // 数据所采用的格式（也有助于定义数据大小）
    attributeDescriptions[0].offset = offsetof(Vertex_u,position);  // 该属性在单个顶点的数据中定义的位置

    // Colour Attribute
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex_u, color);

    //============================================================================================
    // -- 顶点输入 (TODo：在创建资源时添加顶点描述)-
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;     // 顶点绑定描述列表
    vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();// 顶点属性描述列表

    // -- 输入组件 --
    VkPipelineInputAssemblyStateCreateInfo inputAssembly ={};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;    // 将原始类型用于组装顶点
    inputAssembly.primitiveRestartEnable = VK_FALSE;                 // 允许覆盖“条带”拓扑以开始新的原始类型


    //-- 视口与剪刀 --
    //ToDo: 创建视口信息结构
    VkViewport viewport = {};
    viewport.x = 0.0f;                                   // x 起始坐标
    viewport.y = 0.0f;                                   // y 起始坐标
    viewport.width = (float)swapChainExtent.width;       // 视口宽度
    viewport.height = (float)swapChainExtent.height;     // 视口高度
    viewport.minDepth = 0.0f;                            // 最小帧缓冲区深度
    viewport.maxDepth = 1.0f;                            // 最大帧缓冲区深度


    //ToDo: 创建一个剪刀信息结构体
    VkRect2D scissor ={};
    scissor.offset = {0,0};                     // 偏移以使用区域
    scissor.extent = swapChainExtent;                    // 指定要使用的区域范围，从偏移量开始

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo ={};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

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


    // -- 光栅化程序 --
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable=VK_FALSE;         // 如果超出近/远平面的碎片被裁剪（默认）或限制在平面上
    rasterizerCreateInfo.rasterizerDiscardEnable =VK_FALSE; // 是否丢弃数据并跳过光栅化器。从不生成碎片，仅适用于无帧缓冲区输出的管线
    rasterizerCreateInfo.polygonMode =VK_POLYGON_MODE_FILL; // 如何处理顶点之间的填充点
    rasterizerCreateInfo.lineWidth =1.0f;                   // 绘制时线条的粗细
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;  // 三角形应剔除哪一面
    rasterizerCreateInfo.frontFace =VK_FRONT_FACE_COUNTER_CLOCKWISE;// 指定绕行方式以确定哪一侧为正面
    rasterizerCreateInfo.depthBiasEnable =VK_FALSE;         // 是否为碎片添加深度偏移量（有助于在阴影映射中防止“阴影痤疮”）


    // -- 多采样 --
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
    multisamplingCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable=VK_FALSE;               // 启用多重采样着色或不启用
    multisamplingCreateInfo.rasterizationSamples =VK_SAMPLE_COUNT_1_BIT;// 每片段使用的采样数量


    //ToDo: 混合附件状态（混合方式的处理）
    VkPipelineColorBlendAttachmentState colourState = {};
    colourState.colorWriteMask =VK_COLOR_COMPONENT_R_BIT |VK_COLOR_COMPONENT_G_BIT// Colours to apply blending to
    |VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    colourState.blendEnable =VK_TRUE;// Enable blending

    //ToDo: 混合使用公式：(srcColorBlendFactor * 新颜色) + colorBlendOp (dstColorBlendFactor * 旧颜色)
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

    // 摘要：(1 * 新阿尔法) + (e * 旧阿尔法) = 新阿尔法
    //
    //-- 管道布局（待用：应用未来的描述符集布局） --
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo ={};
    pipelineLayoutCreateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount =1;
    pipelineLayoutCreateInfo.pSetLayouts =&descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount =0;
    pipelineLayoutCreateInfo.pPushConstantRanges =nullptr;


    VkResult result =vkCreatePipelineLayout(logicalDevice,&pipelineLayoutCreateInfo,nullptr,&pipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Pipeline Layout!");
    }


    // -- 图形管线创建 --
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colourBlendingCreateInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;

    pipelineCreateInfo.basePipelineHandle =VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex =-1;

    // 创建图形管线
    result =vkCreateGraphicsPipelines(logicalDevice,VK_NULL_HANDLE,1,&pipelineCreateInfo,nullptr,&graphicsPipeline);
    if (result !=VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Graphics Pipeline!");
    }

    //-- 销毁着色器模块 --
    vkDestroyShaderModule(logicalDevice,fragmentShaderModule,nullptr);
    vkDestroyShaderModule(logicalDevice,vertexShaderModule,nullptr);
}
//10-帧缓冲
void VulkanRenderer::createFramebuffers_10()
{
    swapChainFramebuffers.resize(swapChainImages.size());

    // 为每个交换链图像创建一个帧缓冲区
    for (size_t i=0;i<swapChainFramebuffers.size();i++)
    {
        std::array<VkImageView,1> attachments ={
            swapChainImages[i].imageView
        };

        VkFramebufferCreateInfo framebufferCreateInfo= {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();// 附件列表（与渲染通道一一对应）
        framebufferCreateInfo.width = swapChainExtent.width;    // 纹理缓冲区宽度
        framebufferCreateInfo.height = swapChainExtent.height;   // 纹理缓冲区高度
        framebufferCreateInfo.layers = 1;                        // 纹理缓冲区图层

        VkResult result = vkCreateFramebuffer(logicalDevice,&framebufferCreateInfo,nullptr,&swapChainFramebuffers[i]);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a Framebuffer!");
        }
    }
}
//11-命令池
void VulkanRenderer::createCommandPool_11()
{
    // 从设备获取队列家族的索引
    QueueFamilyIndices_u queueFamilyIndices = getQueueFamilies_56A_(physicalDevice);

    VkCommandPoolCreateInfo poolInfo= {};
    poolInfo.sType =VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
    // 允许重置/重新录制命令缓冲区（ImGui 的绘制数据每帧都会变化，命令缓冲区需要每帧重录）
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    // 创建图形队列家族命令池
    VkResult result =vkCreateCommandPool(logicalDevice,&poolInfo,nullptr,&graphicsCommandPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Command Pool!");
    }
}
//12-命令缓冲区
void VulkanRenderer::createCommandBuffers_12()
{
    // 将命令缓冲区数量调整为每个帧缓冲区一个
    commandBuffers.resize(swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo cbAllocInfo ={};
    cbAllocInfo.sType =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAllocInfo.commandPool = graphicsCommandPool;
    cbAllocInfo.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAllocInfo.commandBufferCount =static_cast<uint32_t>(commandBuffers.size());

    // 分配命令缓冲区并将句柄放入缓冲区数组中
    VkResult result = vkAllocateCommandBuffers(logicalDevice,&cbAllocInfo, commandBuffers.data());
    if (result !=VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Command Buffers!");
    }

    // 每个命令缓冲区都记录一个栅栏，表示它最近一次被提交时所用的栅栏
    commandBufferFences.resize(commandBuffers.size(), VK_NULL_HANDLE);
}
//13-排布每个模型的矩阵
void VulkanRenderer::allocateDynamicBufferTransferSpace_13()
{
    // 计算模型数据的对齐情况
    modelUniformAlignment = (sizeof(UboModel) + minUniformBufferOffset - 1)
                            & ~(minUniformBufferOffset - 1);

    // 在内存中创建空间，用于存放与所需对齐的动态缓冲区，并可容纳 MAX_OBJECTS 个对象。
    modelTransferSpace = (UboModel *)_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment);
}
//14-在 GPU 侧建缓冲区
void VulkanRenderer::createUniformBuffers_14()
{
    // ViewProjection buffer size
    VkDeviceSize vpBufferSize = sizeof(UBO_VP);

    // Model buffer size
    VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS;

    // One uniform buffer for each image (and by extension, command buffer)
    vpUniformBuffer.resize(swapChainImages.size());
    vpUniformBufferMemory.resize(swapChainImages.size());
    modelDUniformBuffer.resize(swapChainImages.size());
    modelDUniformBufferMemory.resize(swapChainImages.size());

    // Create Uniform buffers
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        createBuffer_u(physicalDevice, logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffer[i], &vpUniformBufferMemory[i]);

        createBuffer_u(physicalDevice, logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelDUniformBuffer[i], &modelDUniformBufferMemory[i]);
    }
}
//15-创建描述符池
void VulkanRenderer::createDescriptorPool_15()
{
    // 描述符类型 + 描述符数量，而非描述符集（组合后构成池的大小）
    // 视图投影池
    VkDescriptorPoolSize vpPoolSize = {};
    vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffer.size());

    // 模型池（动态）
    VkDescriptorPoolSize modelPoolSize = {};
    modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    modelPoolSize.descriptorCount = static_cast<uint32_t>(modelDUniformBuffer.size());

    // 池塘尺寸列表
    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = { vpPoolSize, modelPoolSize };

    // 用于创建描述符池的数据
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size());					// 可从池中创建的最大描述符集数量
    poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());		// 传递的池大小数量
    poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();									// 要创建池的池大小

    // 创建描述符池
    VkResult result = vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Descriptor Pool!");
    }
}
//16-创建描述符集
void VulkanRenderer::createDescriptorSets_16()
{
    // 调整描述符集列表的大小，使其为每个缓冲区各一个
    descriptorSets.resize(swapChainImages.size());

    std::vector<VkDescriptorSetLayout> setLayouts(swapChainImages.size(), descriptorSetLayout);

    // 描述符集分配信息
    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = descriptorPool;									// 用于分配描述符集的池
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());// 要分配的描述符集数量
    setAllocInfo.pSetLayouts = setLayouts.data();									// 用于分配描述符集的布局（1:1 关系）

    // 分配描述符集（多个）
    VkResult result = vkAllocateDescriptorSets(logicalDevice, &setAllocInfo, descriptorSets.data());
    if (result != VK_SUCCESS)
    {
    	throw std::runtime_error("Failed to allocate Descriptor Sets!");
    }

    // 更新所有描述符集缓冲区绑定
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
	    // 视图投影描述符
	    // 缓冲区信息和数据偏移量信息
	    VkDescriptorBufferInfo vpBufferInfo = {};
	    vpBufferInfo.buffer = vpUniformBuffer[i];		// 缓冲区以获取数据
	    vpBufferInfo.offset = 0;						// 数据起始位置
	    vpBufferInfo.range = sizeof(UBO_VP);			// 数据大小

        // 关于绑定与缓冲区之间连接的数据
	    VkWriteDescriptorSet vpSetWrite = {};
	    vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	    vpSetWrite.dstSet = descriptorSets[i];								// 要更新的描述符集
	    vpSetWrite.dstBinding = 0;											// 要更新的绑定（与布局/着色器上的绑定匹配）
	    vpSetWrite.dstArrayElement = 0;									    // 数组中要更新的索引
	    vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;		// 描述符类型
	    vpSetWrite.descriptorCount = 1;									    // 要更新的数量
	    vpSetWrite.pBufferInfo = &vpBufferInfo;							    // 关于要绑定的缓冲区数据的信息

	    // 模型描述符
	    // 模型缓冲区绑定信息
	    VkDescriptorBufferInfo modelBufferInfo = {};
	    modelBufferInfo.buffer = modelDUniformBuffer[i];
	    modelBufferInfo.offset = 0;
	    modelBufferInfo.range = modelUniformAlignment;

	    VkWriteDescriptorSet modelSetWrite = {};
	    modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	    modelSetWrite.dstSet = descriptorSets[i];
	    modelSetWrite.dstBinding = 1;
	    modelSetWrite.dstArrayElement = 0;
	    modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	    modelSetWrite.descriptorCount = 1;
	    modelSetWrite.pBufferInfo = &modelBufferInfo;

        // 描述符集写入列表
	    std::vector<VkWriteDescriptorSet> setWrites = { vpSetWrite, modelSetWrite };

        // 更新描述符集以包含新的缓冲区/绑定信息
	    vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(),
							0, nullptr);
    }
}
//17-录制命令
void VulkanRenderer::recordCommands_17(uint32_t imageIndex)
{
    // Information about how to begin each command buffer
    VkCommandBufferBeginInfo bufferBeginInfo = {};
    bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // Information about how to begin a render pass (only needed for graphical applications)
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;							// Render Pass to begin
    renderPassBeginInfo.renderArea.offset = { 0, 0 };						// Start point of render pass in pixels
    renderPassBeginInfo.renderArea.extent = swapChainExtent;				// Size of region to run render pass on (starting at offset)
    VkClearValue clearValues[] = {
    	{0.6f, 0.65f, 0.4, 1.0f}
    };
    renderPassBeginInfo.pClearValues = clearValues;							// List of clear values (TODO: Depth Attachment Clear Value)
    renderPassBeginInfo.clearValueCount = 1;

    // 只录制当前获取到的那张交换链图像对应的命令缓冲区
    {
    	uint32_t i = imageIndex;
    	renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];

    	// Start recording commands to command buffer!
    	VkResult result = vkBeginCommandBuffer(commandBuffers[i], &bufferBeginInfo);
    	if (result != VK_SUCCESS)
    	{
    		throw std::runtime_error("Failed to start recording a Command Buffer!");
    	}

    		// Begin Render Pass
    		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    			// Bind Pipeline to be used in render pass
    			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    			for (size_t j = 0; j < meshList.size(); j++)
    			{
    				VkBuffer vertexBuffers[] = { meshList[j].getVertexBuffer() };					// Buffers to bind
    				VkDeviceSize offsets[] = { 0 };												// Offsets into buffers being bound
    				vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);	// Command to bind vertex buffer before drawing with them

    				// Bind mesh index buffer, with 0 offset and using the uint32 type
    				vkCmdBindIndexBuffer(commandBuffers[i], meshList[j].getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    				// Dynamic Offset Amount
    				uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;

    				// Bind Descriptor Sets
    				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
    					0, 1, &descriptorSets[i], 1, &dynamicOffset);

    				// Execute pipeline
    				vkCmdDrawIndexed(commandBuffers[i], meshList[j].getIndexCount(), 1, 0, 0, 0);
    			}

    		// 绘制 ImGui 界面（必须在渲染通道结束之前）
    		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers[i]);

    		// End Render Pass
    		vkCmdEndRenderPass(commandBuffers[i]);

    	// Stop recording to command buffer
    	result = vkEndCommandBuffer(commandBuffers[i]);
    	if (result != VK_SUCCESS)
    	{
    		throw std::runtime_error("Failed to stop recording a Command Buffer!");
    	}
    }
}
//18-信号量和栅栏
void VulkanRenderer::createSynchronisation_18()
{
    imageAvailable.resize(MAX_FRAME_DRAWS);
    renderFinished.resize(MAX_FRAME_DRAWS);
    drawFences.resize(MAX_FRAME_DRAWS);

    // Semaphore creation information
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Fence creation information
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        if (vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &drawFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a Semaphore and/or Fence!");
        }
    }
}





//====================================================================================================
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
    for (const auto &checkExtension : *checkExtensions)
    {
        bool hasExtension = false;
        for (const auto &extension : extensions)
        {
            if (strcmp(checkExtension, extension.extensionName) == 0)
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
//检查驱动扩展
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
    for (const auto& deviceExtension : deviceExtensions_u)
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
}
//Null
bool VulkanRenderer::checkValidationLayerSupport()
{
    //获取可用的验证层数量
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    //填充可用验证层列表
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    //检查我们要使用的每一个验证层是否都可用
    for (const auto& layerName : validationLayers)
    {
        bool hasLayer = false;
        for (const auto& layerProperty : availableLayers)
        {
            if (strcmp(layerName, layerProperty.layerName) == 0)
            {
                hasLayer = true;
                break;
            }
        }
        if (!hasLayer)
        {
            printf("Validation layer not available: %s\n", layerName);
            return false;
        }
    }
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
//====================================================================================================
//— 查某块设备的队列家族，定位支持图形命令的 graphicsFamily 和支持呈现到窗口的 presentFamily 索引
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
            physicalDevice = device;
            break;
        }
        i++;
    }
    return indices;
}
//— 查该设备在当前表面上的能力：可用像素格式、呈现模式和分辨率范围，后面挑选参数靠它
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
//====================================================================================================
//从可用格式里挑最合适的：优先 R8G8B8A8 / B8G8R8A8 加 sRGB 色彩空间，否则退回第一个
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
//挑呈现模式：优先 MAILBOX（三缓冲、低延迟），找不到就退回规范强制支持的 FIFO
VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR> presentationModes)
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
//决定交换链图像分辨率：能力给了固定值就用它，否则取窗口帧缓冲大小并夹到 min/max 范围内
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
//====================================================================================================
//为图像创建 2D 视图。交换链拿到的是裸图像，必须包一层视图才能被帧缓冲和管线使用
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
    VkResult result =vkCreateImageView(logicalDevice,&viewCreateInfo,nullptr,&imageView);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an Image View!");
    }

    return imageView;

}
//把读入的 SPIR-V 字节码包装成着色器模块，作为图形管线各阶段的可执行代码来源
VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
    // 着色器模块创建信息
    VkShaderModuleCreateInfo shaderModuleCreateInfo={};
    shaderModuleCreateInfo.sType =VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize =code.size();
    // 代码大小
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
    // 指向代码的指针（uint32_t 指针类型）
    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(logicalDevice,&shaderModuleCreateInfo,nullptr,&shaderModule);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a shader module!");
    }
    return shaderModule;
};
//====================================================================================================


void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{
    // 复制VP数据
    void * data;
    vkMapMemory(logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UBO_VP), 0, &data);
    memcpy(data, &ubo_VP, sizeof(UBO_VP));
    vkUnmapMemory(logicalDevice, vpUniformBufferMemory[imageIndex]);

    // 复制模型数据
    for (size_t i = 0; i < meshList.size(); i++)
    {
        UboModel * thisModel = (UboModel *)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
        *thisModel = meshList[i].getModel();
    }

    // 映射模型数据列表
    vkMapMemory(logicalDevice, modelDUniformBufferMemory[imageIndex], 0, modelUniformAlignment * meshList.size(), 0, &data);
    memcpy(data, modelTransferSpace, modelUniformAlignment * meshList.size());
    vkUnmapMemory(logicalDevice, modelDUniformBufferMemory[imageIndex]);
}