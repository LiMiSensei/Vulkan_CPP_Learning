//
// Created by LiMi on 2026/9/14.
//

#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer() {
}

int VulkanRenderer::init(GLFWwindow* window) {
    this->window = window;

    try {
        createInstance_1(); //执行实例化Vulkan函数
        createDebugCallback_(); //
        createSurface_2(); //
        getPhysicalDevice_2(); //获取物理设备
        createLogicalDevice_3(); //创建逻辑设备
    } catch (const std::runtime_error& e) {
        printf("ERROR==: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::cleanup() {
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(mainDevice.logicalDevice, nullptr);
    /*if (validationEnabled) {
        DestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);
    }*/
    vkDestroyInstance(instance, nullptr);
}

VulkanRenderer::~VulkanRenderer() {
}

//1-创建实例
void VulkanRenderer::createInstance_1() {
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
    std::vector<const char *> instanceExtensions = std::vector<const char *>();

    //设置扩展实例将使用
    uint32_t glfwExtensionCount = 0; //GLFW可能需要多个扩展
    const char** glfwExtensions; //扩展作为字符串数组传递，因此需要指针（该数组）指向指针（该字符串）

    //获取GLFW扩展
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    //将GLFW扩展添加到Vkinstance扩展列表中
    for (size_t i = 0; i < glfwExtensionCount; i++) {
        instanceExtensions.push_back(glfwExtensions[i]);
    }

    //检查实例扩展支持
    if (!checkInstanceExtensionSupport_(&instanceExtensions)) {
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

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}

void VulkanRenderer::createDebugCallback_() {
}


//3-创建逻辑设备
void VulkanRenderer::createLogicalDevice_3() {
    //获取所选物理设备的队列家族索引
    QueueFamilyIndices_ indices = getQueueFamilies_(mainDevice.physicalDevice);

    // Vector for queue creation information, and set for family indices
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilyIndices = {indices.graphicsFamily, indices.presentFamily};

    //队列逻辑设备需要创建的信息（目前仅支持一个，后续将增加更多！）
    for (int queueFamily: queueFamilyIndices) {
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
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()); //队列创建信息数量
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data(); //队列创建信息列表，以便设备可以创建所需的队列
    deviceCreateInfo.queueCreateInfoCount = 0; //已启用的逻辑设备扩展数量
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data(); //已启用的逻辑设备扩展列表
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    //为给定的物理设备创建逻辑设备
    VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }
    //队列与设备同时创建  因此我们希望处理队列。
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface_2() {
    VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}


//2-获取物理设备
void VulkanRenderer::getPhysicalDevice_2() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan instance");
    }
    std::vector<VkPhysicalDevice> devicesList(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devicesList.data());


    for (const auto& device: devicesList) {
        if (checkDeviceSuitable_(device)) {
            mainDevice.physicalDevice = device;
            break;
        }
    }
}

//检查实例拓展
bool VulkanRenderer::checkInstanceExtensionSupport_(std::vector<const char *>* checkExtensions) {
    //需要获取扩展数量以创建正确大小的数组来存储扩展。
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    //使用count创建VkExtensionProperties列表
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    //检查给定的扩展是否在可用扩展列表中
    bool hasExtension = false;
    for (const auto& checkExtension: *checkExtensions) {
        for (const auto& extension: extensions) {
            if (strcmp(checkExtension, extension.extensionName)) {
                hasExtension = true;
                break;
            }
        }
        if (!hasExtension) {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport_(VkPhysicalDevice device) {
    //获取设备扩展数量
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    //如果未找到扩展，返回失败
    if (extensionCount == 0) {
        return false;
    }
    //填充扩展列表
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

    //检查扩展程序
    for (const auto& deviceExtension: deviceExtensions) {
        bool hasExtension = false;
        for (const auto& extension: extensions) {
            if (strcmp(deviceExtension, extension.extensionName) == 0) {
                hasExtension = true;
                break;
            }
        }
        if (!hasExtension) {
            return false;
        }
    }
    return true;

    return true;
}

bool VulkanRenderer::checkValidationLayerSupport() {
    return true;
}

//检查合格设备
bool VulkanRenderer::checkDeviceSuitable_(VkPhysicalDevice device) {
    /*// 关于设备本身的详细信息（ID、名称、类型、厂商等）
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    // 关于设备功能的信息（如几何着色器、细分着色器、粗线等）
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);*/
    QueueFamilyIndices_ indices = getQueueFamilies_(device);
    bool extensionsSupported = checkDeviceExtensionSupport_(device);

    return indices.isVlid() && extensionsSupported;
}

//获取队列家族 （检查合格设备，）
QueueFamilyIndices_ VulkanRenderer::getQueueFamilies_(VkPhysicalDevice device) {
    QueueFamilyIndices_ indices;

    //获取指定设备的所有队列家族属性信息
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

    //遍历每个队列家族，检查其是否至少包含所需类型的队列之一
    int i = 0;
    for (const auto& queueFamily: queueFamilyList) {
        //首先检查队列家族是否至少包含一个队列（可能没有队列)
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i; // 如果队列家族有效，则获取索引
        }
        //检查队列家族是否支持呈现
        VkBool32 presentFamily = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentFamily);
        //检查队列是否为演示类型【（可以是图形和演示的组合）
        if (queueFamily.queueCount > 0 && presentFamily) {
            indices.presentFamily = i;
        }
        //检查队列家族索引是否处于有效状态，如果是则停止搜索
        if (indices.isVlid()) {
            mainDevice.physicalDevice = device;
            break;
        }
        i++;
    }
    return indices;
}
