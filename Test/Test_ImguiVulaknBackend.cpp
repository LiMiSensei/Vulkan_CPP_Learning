//
// Created by LiMi on 2026/10/2.
//

#include "Test_ImguiVulaknBackend.h"

#include <iostream>


static void DebugLog(VkResult result, const char* message) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(message);
    }
}

// ImGui 后端内部每次 Vulkan 调用都会回调这里。必须设置它：
// 不设置的话 ImGui 1.92 的 check_vk_result 只是个 IM_ASSERT，Release 下等于空操作，
// 管线 / 描述符创建失败会被静默吞掉，之后拿着无效句柄去绑定，直接访问违规且毫无提示。
static void 检查Vk结果(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        std::cerr << "[imgui/vulkan] VkResult = " << static_cast<int>(result) << std::endl;
    }
}

//换成"纯黑"主题：先要一套 ImGui 自带的暗色配色打底（保证每个 ImGuiCol_ 都有值），
//再把背景、控件底全都压到接近全黑，层次只靠边框亮度、文字白度区分。
//只在初始化时调一次：主题是全局状态，每帧重设既浪费也会盖掉运行时的改动。
static void 应用黑色主题()
{
    ImGui::StyleColorsDark();   // 打底

    ImGuiStyle& style = ImGui::GetStyle();

    //1-外形：直角为主，更像工具面板；边框打开一点，黑底上才看得出控件的边界
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 2.0f;
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(6.0f, 3.0f);
    style.ItemSpacing       = ImVec2(6.0f, 5.0f);

    ImVec4* c = style.Colors;

    //2-各种"底"：接近纯黑；面板留一点点透明度，背后的 3D 场景还能透出来
    c[ImGuiCol_WindowBg]         = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);
    c[ImGuiCol_ChildBg]          = ImVec4(0.04f, 0.04f, 0.04f, 0.00f);
    c[ImGuiCol_PopupBg]          = ImVec4(0.05f, 0.05f, 0.05f, 0.98f);
    c[ImGuiCol_MenuBarBg]        = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
    c[ImGuiCol_DockingEmptyBg]   = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    //3-标题栏 / 折叠头 / 列表选中项：比背景亮一档表示"当前"
    c[ImGuiCol_TitleBg]          = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    c[ImGuiCol_Header]           = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    c[ImGuiCol_HeaderActive]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

    //4-输入框 / 按钮：黑底 + 细灰边，鼠标经过才亮
    c[ImGuiCol_FrameBg]          = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_Border]           = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_BorderShadow]     = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);   // 不要投影，纯黑主题加投影会脏
    c[ImGuiCol_Button]           = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);

    //5-滑块手柄 / 勾选标记：黑底上就靠它们提供"可以拖 / 已经勾上"的对比
    c[ImGuiCol_SliderGrab]       = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    c[ImGuiCol_CheckMark]        = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);

    //6-滚动条：轨道比背景更黑，滑块用灰
    c[ImGuiCol_ScrollbarBg]      = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]    = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);

    //7-分隔线 / 标签页 / 停靠预览
    c[ImGuiCol_Separator]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_Tab]              = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    c[ImGuiCol_TabHovered]       = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    c[ImGuiCol_TabSelected]      = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    c[ImGuiCol_TabDimmed]        = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]= ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_DockingPreview]   = ImVec4(0.35f, 0.35f, 0.35f, 0.70f);

    //8-选中文字的背景
    c[ImGuiCol_TextSelectedBg]   = ImVec4(0.30f, 0.30f, 0.30f, 0.60f);
}

Test_ImguiVulaknBackend::Test_ImguiVulaknBackend()
{
}

Test_ImguiVulaknBackend::Test_ImguiVulaknBackend(GLFWwindow* window, VkInstance instance,VkPhysicalDevice physicalDevice,
VkDevice device, uint32_t graphicsFamily, uint32_t imageCount, VkQueue graphics,VkRenderPass renderPass,
VkSampleCountFlagBits sampleCount)
            : window(window),instance(instance),physicalDevice(physicalDevice),device(device),
            graphicsFamily(graphicsFamily), imageCount(imageCount),graphics(graphics), renderPass(renderPass),
            sampleCount(sampleCount)
{
    // 创建 ImGui 上下文
    ImGui::CreateContext();
    //ImGui::StyleColorsDark();
    应用黑色主题();                          // 纯黑主题（只在这里设一次，draw() 里不再重设）

    // 默认字体只含 ASCII 字形，中文找不到字形就会画成 "?"：换成系统中文字体
    // ImGui 1.92 起字形是按需光栅化的，不需要再传 GlyphRanges
    ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f); // 微软雅黑

    // 开启窗口停靠
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // 平台后端（GLFW）：用 Vulkan 渲染，所以是 ForVulkan
    ImGui_ImplGlfw_InitForVulkan(window, true); //

    // 渲染后端（Vulkan）
    ImGui_ImplVulkan_InitInfo initInfo = {};                // 渲染后端初始化信息
    initInfo.Instance = instance;                           // Vulkan 实例
    initInfo.PhysicalDevice = physicalDevice;               // 选中的物理设备
    initInfo.Device = device;                               // 逻辑设备
    initInfo.QueueFamily = graphicsFamily;                  // 图形队列家族下标
    initInfo.Queue = graphics;                              // 提交命令用的队列
    initInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE; // 让后端自己建描述符池
    initInfo.MinImageCount = 2;                             // 最少图像数量
    initInfo.ImageCount = imageCount;                       // 交换链图像数量
    initInfo.PipelineInfoMain.RenderPass = renderPass;      // 共用的渲染通道
    initInfo.PipelineInfoMain.Subpass = 0;                  // 子通道下标
    initInfo.PipelineInfoMain.MSAASamples = sampleCount;    // 多重采样数
    initInfo.CheckVkResultFn = 检查Vk结果;                   // 后端内部 Vulkan 调用出错时打出来，别静默吞掉

    // 初始化失败则报错
    if (!ImGui_ImplVulkan_Init(&initInfo))
    {
        throw std::runtime_error("Failed to initialize ImGui Vulkan backend!");
    }
}

Test_ImguiVulaknBackend::~Test_ImguiVulaknBackend()
{
}

//只搬 Vulkan 句柄；modelGUI / shaderSystem / hierarchy 保持原样，
//因为它们内部的引用必须一直指向本对象的 modelGUI（见头文件注释）
Test_ImguiVulaknBackend& Test_ImguiVulaknBackend::operator=(const Test_ImguiVulaknBackend& other)
{
    if (this == &other) return *this;

    window         = other.window;
    instance       = other.instance;
    physicalDevice = other.physicalDevice;
    device         = other.device;
    graphicsFamily = other.graphicsFamily;
    imageCount     = other.imageCount;
    graphics       = other.graphics;
    renderPass     = other.renderPass;
    sampleCount    = other.sampleCount;

    return *this;
}




void Test_ImguiVulaknBackend::draw()
{
    //1-ImGui Vulkan & ImGui GLFW 后端新帧
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    //2-开始一个 ImGui 帧
    ImGui::NewFrame();
    //3-全屏停靠空间（中央透明）
    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    //=======================================================================================
    meshGUI.Draw(window);
    shaderSystemGUI.Draw(window);
    hierarchy.Draw();
    //=======================================================================================
    //4-生成 ImGui 绘制数据
    ImGui::Render();
}
