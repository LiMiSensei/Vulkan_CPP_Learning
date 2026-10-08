//
// Created by LiMi on 2026/10/3.
//

#ifndef VULKAN_CPP_LEARNING_GUI_SHADERSYSTEM_H
#define VULKAN_CPP_LEARNING_GUI_SHADERSYSTEM_H
#include <vulkan/vulkan_core.h>

#include "../Header/Other.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>   // 用它拿 GLFW 窗口的 HWND，让文件对话框挂在窗口上
#include <commdlg.h>
#include <string>             // std::string（读取文件要返回文本）
#include <utility>            // std::pair（共享片段表是 名字->正文 的键值对）
#include <vector>             // std::vector

#include "GUI_Mesh.h"
#include "GLFW/glfw3.h"       // GLFWwindow（文件选择器要挂在窗口上）


class GUI_ShaderSystem
{
private:
    // 共享片段表：片段名 -> 正文。.shader 里写 #include <名字> 的那一行，编译前会被整段换成这里的正文。
    // 为什么要有这张表：DXC 的 #include 只认磁盘上的真实文件，而这里要的是"引用内置片段"，
    // 所以只能自己在送进编译器之前展开。
    // 每个片段的成员顺序必须和 TestRender.h 的 CameraUniformBuffer 一字不差，否则 CPU 写进去的数据全错位
    std::vector<std::pair<std::string, std::string>> 共享片段 = {{ "UniformBuffer", R"(
cbuffer CameraUniformBuffer : register(b0, space0)
{
    float4 _Time;
    float4 _MainLightColor;
    float4 _MainLightDirection;
    float4 _WorldSpaceCameraPos;
    float4x4 _MATRIX_V;
    float4x4 _MATRIX_P;

};
)" },};

    std::vector<Other1::Material> materials;
    int selseID = -1;
    GUI_Mesh& model_gui;   // 必须是引用：存成值只会拷贝一份快照，选中态和改过的材质都写不回真正的模型列表
public:
    GUI_ShaderSystem(GUI_Mesh& model);
    void Draw(GLFWwindow* window);
private:
    //3-工具
    std::string 读取原文本(GLFWwindow* window);//返回原始Text
    std::string 去除文本注释(std::string text);//去除文本中的注释
    std::string 读取花括号内容(std::string beginning,std::string text);
    std::string 展开共享片段(const std::string& text);   // 把 #include <名字> 换成共享片段表里的正文
    //2-读取Shader的GUI
    Other1::GUITest 读取GUI面板(const std::string text);
    void 调试GUI(Other1::GUITest gui);
    //3-读取渲染状态
    Other1::RenderState 解析渲染状态(const std::string text);//返回RenderState

    //4-读取Shader相关
    Other1::Shader 读取顶点和片段着色器(std::string text);
    std::string 读取着色主函数名字(std::string beginning,std::string text);

    std::string 修改顶点输入名称(std::string text);
    std::vector<Other1::VerInputDes> 读取顶点输入数据(std::string text);

    std::vector<char> 编译着色器(std::string text, std::string 入口名, bool 是顶点);//将HLSL编译SPV

    //4-
    void 添加到Material(int seleID,std::vector<Other1::Model> materials);
    void 生成GUI面板();
};


#endif //VULKAN_CPP_LEARNING_GUI_SHADERSYSTEM_H
