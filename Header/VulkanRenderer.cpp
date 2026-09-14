//
// Created by LiMi on 2026/9/14.
//

#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer() {
}

int VulkanRenderer::init(GLFWwindow* window) {
    this->window = window;

    try {
        createInstance();//执行实例化Vulkan函数
        getPhysicalDevice();//获取GPU
    } catch (const std::runtime_error& e) {
        printf("ERROR: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::cleanup() {
    vkDestroyInstance(instance, nullptr);
}

VulkanRenderer::~VulkanRenderer() {
    //枚举vklnstance可以访问的物理设备
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount,nullptr);

    //获取物理设备列表
    std::vector<VkPhysicalDevice> devicesList(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devicesList.data());
}

void VulkanRenderer::createInstance() {
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
    if (!checkInstanceExtensionSupport(&instanceExtensions)) {//内部函数
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

void VulkanRenderer::getPhysicalDevice() {

}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char *>* checkExtensions) {
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
