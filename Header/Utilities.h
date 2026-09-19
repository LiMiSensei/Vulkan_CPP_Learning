//
// Created by LiMi on 2026/9/15.
//

#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>
#include <fstream>

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

static std::vector<char> readFile_u(const std::string &filename)
{
    // 从指定文件打开流
    // std::ios::binary 告诉流以二进制方式读取文件
    // std::ios::ate 告诉流从文件末尾开始读取
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    // 检查文件流是否成功打开
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file!");
    }

    // 获取当前读取位置，用于调整文件缓冲区大小
    size_t fileSize =(size_t)file.tellg();
    std::vector<char> fileBuffer(fileSize);
    // 移动读取位置（跳转到）文件开头
    file.seekg(0);
    // 将文件数据读入缓冲区（总大小为“fileSize”字节的流）
    file.read(fileBuffer.data(),fileSize);

    return fileBuffer;
}