//
// Created by LiMi on 2026/10/2.
//

#pragma once
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>   // 用它拿 GLFW 窗口的 HWND，让文件对话框挂在窗口上
#include <commdlg.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>   // glm::inverseTranspose：算法线矩阵


#include <commdlg.h>
#include "../Header/Other.h"
#include "assimp/postprocess.h"
#include <memory>
#include "assimp/Importer.hpp"



using namespace Other1;

class GUI_Mesh
{
private:

public:
    void Draw(GLFWwindow* window);
public:
    //模型列表
    std::vector<Model> models;
    //当前选择
    int selectedModel = -1;

private:
    std::string 打开文件选择器(GLFWwindow* window);
    // 整个模型统一归一化：所有子网格当"一个整体"算包围盒，中心移到原点、最长边缩到目标尺寸。
    // 不能一个子网格一个包围盒——那会让每个部件各自缩到同样大小、又各自落到原点，全堆在一起
    void 归一化模型(std::vector<Mesh>& meshes);
    void 删除选中子网格();   // 删除列表里当前选中的那个子网格（selectedModel 是"跨所有模型"的统一编号）
    bool 填充模型数据(std::string path,Other1::Model& m);

    // 取出当前选中的子网格。用"指针的引用"当输出参数：调用方拿到的是 models 里那份真对象，
    // 改它（比如赋材质）才写得回去；传 Mesh* 或 Mesh& 都会得到一份拷贝/空值
    bool 返回选中对象(int& seleMeshID, Mesh*& m);
};



