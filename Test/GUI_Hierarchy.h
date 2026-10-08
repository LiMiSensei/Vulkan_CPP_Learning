//
// Created by LiMi on 2026/10/6.
//

#ifndef VULKAN_CPP_LEARNING_GUI_HIERARCHY_H
#define VULKAN_CPP_LEARNING_GUI_HIERARCHY_H
#include "GUI_Mesh.h"
#include "../Header/Other.h"


class GUI_Hierarchy
{
    public:
    Camera camera;
    MainLight mainLight;
    GUI_Mesh& model;
public:
    GUI_Hierarchy(GUI_Mesh& model);
    void Draw();
    void 处理相机输入(GLFWwindow* window);   // 右键拖拽转视角，WASD 平移相机，Q / E 沿世界 Y 轴升降
    void 处理灯光信息();
    // 每帧检测窗口帧缓冲尺寸和交换链尺寸是否一致（和 处理相机输入 一样，属于"窗口相关"的每帧处理）。
    // 返回 true  = 尺寸变了，调用方该重建交换链；
    // 返回 false = 尺寸没变；窗口最小化（帧缓冲尺寸为 0）时这里会阻塞等事件，也返回 false
    bool 检测尺寸变化(GLFWwindow* window, VkExtent2D 交换链尺寸);
};


#endif //VULKAN_CPP_LEARNING_GUI_HIERARCHY_H
