#include <GLFW/glfw3.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //这实际上定义了glm的裁剪 0-1
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <iostream>
#include <ostream>
#include "Header/VulkanRenderer.h"



GLFWwindow* window;
VulkanRenderer renderer;


void initWindow(std::string windowName = "TestWindow", int width = 800, int height = 600) {
    //初始化GLFW
    glfwInit();
    //设置GLFW不与OpenGL一起工作
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_RESIZABLE);

    window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
}

int main() {

    //创建窗口
    initWindow();

    //创建Vulkan渲染器实例
    renderer.init(window);

    //循环直到关闭
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

    }

    //销毁GLFW窗口并停止GLFW
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;

}
