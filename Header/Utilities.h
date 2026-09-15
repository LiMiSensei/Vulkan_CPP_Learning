//
// Created by LiMi on 2026/9/15.
//

#ifndef VULKAN_CPP_LEARNING_UTILITIES_H
#define VULKAN_CPP_LEARNING_UTILITIES_H
#include <vector>
#include <vulkan/vulkan_core.h>


class Utilities {
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
//队列家族的索引（位置）、（如果它们存在的话）
struct QueueFamilyIndices_ {
    int graphicsFamily = -1; //图形队列家族的位置
    int presentFamily = -1;  //Location of Presentation Queue Family
    bool isVlid() {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

#endif //VULKAN_CPP_LEARNING_UTILITIES_H
