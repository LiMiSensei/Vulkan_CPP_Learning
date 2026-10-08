// ============================================================================
// 单文件 Vulkan 渲染示例（不使用 Header 目录里的任何文件）
//   - 用 Assimp 加载 Modes/2.fbx、Modes/Test.fbx（构建时拷成 exe 旁边的 model.fbx / test.fbx）
//     一起渲染，带 MVP 矩阵、深度缓冲、背面剔除
//   - 按住鼠标右键拖动：旋转相机
//   - W / A / S / D：前进 / 左移 / 后退 / 右移
//   - ImGui 显示相机信息
// 依赖：GLFW 3.5.1、GLM 1.0.3、ImGui 1.92.9b、Assimp 6.0.5，着色器 shaders/vert.spv、frag.spv
// ============================================================================

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Windows 的文件选择器（GetOpenFileNameW）要用到 windows.h：
// NOMINMAX 必须先定义，否则 windows.h 里的 min/max 宏会跟 std::max / glm::max 冲突
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>   // 用它拿 GLFW 窗口的 HWND，让文件对话框挂在窗口上
#include <commdlg.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Vulkan 的深度范围是 0~1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>   // glm::inverseTranspose：算法线矩阵

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "Test/TestRender.h"


void 初始化窗口(GLFWwindow*& window)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(800, 600, "Vulkan Model Viewer", nullptr, nullptr); // 创建窗口并保存句柄
    if (window == nullptr) throw std::runtime_error("Failed to create GLFW window!");
}

int main()
{
    // 控制台默认代码页是 936(GBK)，直接打印 UTF-8 文本里的中文会变乱码；改成 UTF-8 才能正确显示
    SetConsoleOutputCP(CP_UTF8);

    GLFWwindow* window;
    初始化窗口(window);

    TestRender render;

    render.初始化函数(window);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        // 必须包一层：渲染里任何 Vulkan 调用失败都由 DebugLog 抛 std::runtime_error，
        // 没人接住就会走 terminate -> abort，退出码是 0xC0000409（__fastfail），
        // 控制台上什么都看不到——连"哪一步挂的"都拿不到，只能靠猜。
        // 接住之后 e.what() 就是 DebugLog 那句话（比如 "6:__7" = vkCreateSwapchainKHR 失败）
        try
        {
            render.渲染函数();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[fatal] " << e.what() << std::endl;
            std::cout.flush();
            std::cerr.flush();
            break;   // 失败后 Vulkan 对象状态多半不可用，继续跑只会连着炸，直接退出
        }
    }
    render.清理函数();
    glfwDestroyWindow(window);
}
