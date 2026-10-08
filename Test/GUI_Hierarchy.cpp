//
// Created by LiMi on 2026/10/6.
//

#include "GUI_Hierarchy.h"
#include <algorithm>   // std::clamp（夹住俯仰角）
#include "imgui.h"


GUI_Hierarchy::GUI_Hierarchy( GUI_Mesh& model):model(model)
{
}

void GUI_Hierarchy::Draw()
{
    ImGui::Begin("Hierarchy");

    //相机：Transform 三项 + fov / 近裁面 / 远裁面，改动能直接写回 camera
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SeparatorText(("相机变换组件:"));
        //1-变换组件
        ImGui::DragFloat3("位置##1", &camera.transform.position.x, 0.01f);
        ImGui::DragFloat3("旋转##1", &camera.transform.rotation.x, 0.5f);
        ImGui::DragFloat3("缩放##1", &camera.transform.scale.x, 0.01f);
        ImGui::SeparatorText(("相机基础参数:"));
        //2-基础参数
        const ImGuiStyle& style = ImGui::GetStyle();
        const float 标签宽 = ImGui::CalcTextSize("近裁剪面").x;   // 近/远裁剪面、移动速度都是 4 个字，宽度一样，共用一份
        const float 输入框宽 = std::max(40.0f,
            ImGui::GetContentRegionAvail().x * 0.5f - 标签宽 - style.ItemInnerSpacing.x - style.ItemSpacing.x);
        ImGui::SetNextItemWidth(输入框宽);
        ImGui::DragFloat("近裁剪面", &camera.nearPlane, 0.01f, 0.001f, camera.farPlane);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(输入框宽);
        ImGui::DragFloat("远裁剪面", &camera.farPlane, 0.1f, camera.nearPlane, 10000.0f);
        ImGui::SetNextItemWidth(输入框宽);
        ImGui::DragFloat("焦距", &camera.fov, 0.5f, 1.0f, 179.0f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(输入框宽);
        ImGui::DragFloat("移动速度", &camera.moveSpeed, 0.05f, 0.1f, 100.0f);   // WASD / Q E 的每秒移动距离
        ImGui::ColorEdit4("清除色",&camera.clearColor.x);
        ImGui::BeginDisabled();
        glm::vec3 lookat = camera.transform.forward();
        ImGui::DragFloat3("Forward", &lookat.x, 0.01f);
        ImGui::EndDisabled();

    }

    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        //1-变换组件
        ImGui::DragFloat3("位置##2", &mainLight.transform.position.x, 0.01f);
        ImGui::DragFloat3("旋转##2", &mainLight.transform.rotation.x, 0.5f);
        ImGui::DragFloat3("缩放##2", &mainLight.transform.scale.x, 0.01f);
        //基础
        ImGui::ColorEdit4("颜色",&mainLight.color.x);


    }

    ImGui::End();
}

//右键拖拽转视角 / WASD 平移相机 / Q E 沿世界 Y 轴升降。
//必须在本帧的 ImGui::NewFrame()（也就是 GUI.draw()）之后调用，
//否则 io.WantCaptureMouse / io.WantCaptureKeyboard 还是上一帧的旧值，判断不准。
void GUI_Hierarchy::处理相机输入(GLFWwindow* window)
{
    const ImGuiIO& io = ImGui::GetIO();

    //0-本帧耗时：按时间推进，移动速度才不会跟着帧率变
    static double 上帧时间 = glfwGetTime();
    const double 当前时间 = glfwGetTime();
    float 帧间隔 = static_cast<float>(当前时间 - 上帧时间);
    上帧时间 = 当前时间;
    帧间隔 = std::clamp(帧间隔, 0.0f, 0.1f);   // 卡顿或断点调试之后别让相机瞬移一大截

    //1-按住右键拖拽转视角：横向绕世界 Y 轴（偏航），纵向绕本地 X 轴（俯仰）
    double 鼠标X = 0.0, 鼠标Y = 0.0;
    glfwGetCursorPos(window, &鼠标X, &鼠标Y);

    // 用上一帧的鼠标位置算增量，就不用把光标隐藏 / 锁死，操作 ImGui 面板也不受影响
    static bool 拖拽中 = false;
    static double 上帧鼠标X = 0.0, 上帧鼠标Y = 0.0;

    const bool 右键按住 = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (!右键按住)
    {
        拖拽中 = false;   // 松开右键，这次拖拽结束
    }
    else if (!拖拽中 && !io.WantCaptureMouse && !io.KeyShift)
    {
        // 只在 3D 区域按下才开始拖拽；开始之后即使鼠标划过面板也继续跟着转，不然手感会断
        // !io.KeyShift：按着 Shift 时右键归灯光（见 处理灯光信息），相机这边不抢，
        // 否则同一组鼠标增量会被两处同时吃掉，相机和灯一起转
        拖拽中 = true;
        上帧鼠标X = 鼠标X;
        上帧鼠标Y = 鼠标Y;
    }

    if (拖拽中)
    {
        constexpr float 灵敏度 = 0.15f;   // 度 / 像素
        // 鼠标右移 -> 视角右转（rotation.y 越小越朝右，见 TestRender::更新相机Uniform 里对 forward 的推导）
        camera.transform.rotation.y -= static_cast<float>(鼠标X - 上帧鼠标X) * 灵敏度;
        // 鼠标下移 -> 视角往下看（rotation.x 越大越朝上）
        camera.transform.rotation.x -= static_cast<float>(鼠标Y - 上帧鼠标Y) * 灵敏度;
        // 俯仰夹在 ±89°：贴到 ±90° 会和 lookAt 的 up=(0,1,0) 共线，视图矩阵退化
        camera.transform.rotation.x = std::clamp(camera.transform.rotation.x, -89.0f, 89.0f);

        上帧鼠标X = 鼠标X;
        上帧鼠标Y = 鼠标Y;
    }

    //2-键盘移动：WASD 沿相机自身的水平朝向走，Q / E 沿世界 Y 轴升降
    if (io.WantCaptureKeyboard) return;   // 正在 ImGui 里输入，别抢按键

    // 只用偏航算前后左右：否则抬着头按 W 会一路往天上飞
    const glm::mat4 偏航 = glm::rotate(glm::mat4(1.0f), glm::radians(camera.transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 前 = glm::vec3(偏航 * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));   // 相机默认朝 -Z 看
    const glm::vec3 右 = glm::vec3(偏航 * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
    const glm::vec3 上(0.0f, 1.0f, 0.0f);                                        // Q / E 走世界空间

    glm::vec3 方向(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) 方向 += 前;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) 方向 -= 前;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) 方向 += 右;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) 方向 -= 右;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) 方向 += 上;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) 方向 -= 上;

    // 归一化是为了斜着按（比如 W+D）不会比单按一个键更快
    if (glm::dot(方向, 方向) > 0.0f)
    {
        camera.transform.position += glm::normalize(方向) * camera.moveSpeed * 帧间隔;
    }
}

//Shift + 右键拖拽转主光方向：左右绕世界 Y 轴偏航，上下绕灯光自身的 X 轴俯仰。
//这里只用 ImGui 的 io 读输入（io.MouseDown / io.KeyShift / io.MouseDelta）：
//鼠标增量 ImGui 每帧自己算好了，不用像 处理相机输入 那样维护"上帧鼠标位置"，也就不需要 GLFWwindow。
//和 处理相机输入 一样，必须在本帧 ImGui::NewFrame() 之后调用，否则 KeyShift / WantCaptureMouse 还是上一帧的值。
void GUI_Hierarchy::处理灯光信息()
{
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.MouseDown[1] || !io.KeyShift || io.WantCaptureMouse) return;
    constexpr float 灵敏度 = 0.15f;   // 度 / 像素，和 处理相机输入 保持一致的手感
    mainLight.transform.rotation.y += io.MouseDelta.x * 灵敏度;
    mainLight.transform.rotation.x += io.MouseDelta.y * 灵敏度;
}


bool GUI_Hierarchy::检测尺寸变化(GLFWwindow* window, VkExtent2D 交换链尺寸)
{
    int 宽 = 0;
    int 高 = 0;
    glfwGetFramebufferSize(window, &宽, &高);

    //0-尺寸为 0 说明窗口被最小化了：Vulkan 不允许建 0 尺寸的交换链。
    //  在事件上睡一觉（不空转 CPU），窗口恢复尺寸后自然返回；这一帧当"没变化"处理
    if (宽 == 0 || 高 == 0)
    {
        glfwWaitEvents();
        return false;
    }

    //1-和交换链尺寸对不上（拖动窗口边框改分辨率）-> 该重建交换链了
    return static_cast<uint32_t>(宽) != 交换链尺寸.width ||
           static_cast<uint32_t>(高) != 交换链尺寸.height;
}
