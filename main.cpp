#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN //这实际上： #include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE //这实际上定义了glm的裁剪 0-1
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <ostream>



int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Hello World", nullptr, nullptr);
    //========================================= 初始化

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    printf("extensionCount:%i\n",extensionCount);

    glm::mat4 testMat = glm::mat4(1.0f);
    glm::vec4 testVec = glm::vec4(1.0f, 2.0f, 3.0f, 4.0f);
    auto testResult = testMat * testVec;

    //========================================= 初始化
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        //========================================= 每帧



        //========================================= 每帧
    }
    //========================================= 结束

    //========================================= 结束
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
