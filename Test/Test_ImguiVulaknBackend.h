//
// Created by LiMi on 2026/10/2.
//

#ifndef VULKAN_CPP_LEARNING_TESTGUI_H
#define VULKAN_CPP_LEARNING_TESTGUI_H
#include <vulkan/vulkan_core.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "GLFW/glfw3.h"
#include <stdexcept>


#include "GUI_Hierarchy.h"
#include "GUI_Mesh.h"
#include "GUI_ShaderSystem.h"

class Test_ImguiVulaknBackend
{
private:
    GLFWwindow* window= VK_NULL_HANDLE;
    VkInstance instance= VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice= VK_NULL_HANDLE;
    VkDevice device= VK_NULL_HANDLE;
    uint32_t graphicsFamily;
    uint32_t imageCount;
    VkQueue graphics = VK_NULL_HANDLE;
    VkRenderPass renderPass = nullptr;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

public:
    GUI_Mesh meshGUI;
    GUI_ShaderSystem shaderSystemGUI = GUI_ShaderSystem(meshGUI);
    GUI_Hierarchy hierarchy = GUI_Hierarchy(meshGUI);
public:
    Test_ImguiVulaknBackend();
    Test_ImguiVulaknBackend(GLFWwindow* window,
    VkInstance instance,
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    uint32_t graphicsFamily,
    uint32_t imageCount,
    VkQueue graphics,
    VkRenderPass renderPass,
    VkSampleCountFlagBits sampleCount
    );

    ~Test_ImguiVulaknBackend();

    Test_ImguiVulaknBackend& operator=(const Test_ImguiVulaknBackend& other);

    void draw();
};


#endif //VULKAN_CPP_LEARNING_TESTGUI_H
