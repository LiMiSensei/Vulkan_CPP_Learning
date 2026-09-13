//
// Created by LiMi on 2026/9/14.
//

#ifndef VULKAN_CPP_LEARNING_VULKANRENDERER_H
#define VULKAN_CPP_LEARNING_VULKANRENDERER_H

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vector>

class VulkanRenderer {
public:
    VulkanRenderer();
    int init(GLFWwindow* window);
    ~VulkanRenderer();

private:
    GLFWwindow* window;
};


#endif //VULKAN_CPP_LEARNING_VULKANRENDERER_H
