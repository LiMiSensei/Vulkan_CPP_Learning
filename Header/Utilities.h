//
// Created by LiMi on 2026/9/15.
//

#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>


/*
class Utilities {
};*/

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

//队列家族的索引（位置）、（如果它们存在的话）
struct QueueFamilyIndices_u {
    int graphicsFamily = -1;                    //图形队列家族的位置
    int presentFamily = -1;                     //演示队列家庭位置
    bool isVlid() {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

struct SwapChainDetails_u {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;         //表面属性，例如图像尺寸/范围
    std::vector<VkSurfaceFormatKHR> formats;            //表面图像格式，例如RGB和每种颜色的大小
    std::vector<VkPresentModeKHR> presentModes;         //图像应如何呈现到屏幕上
};


struct SwapchainImage_u
{
    VkImage image;
    VkImageView imageView;
};


