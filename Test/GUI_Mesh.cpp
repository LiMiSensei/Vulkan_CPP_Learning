//
// Created by LiMi on 2026/10/2.
//

// Windows 文件选择器（GetOpenFileNameW）要用到 windows.h / commdlg.h：
// NOMINMAX 必须在这之前定义，否则 windows.h 的 min/max 宏会跟 std::max 冲突


#include "GUI_Mesh.h"


#include <algorithm>   // std::max（求包围盒最长边）
#include <charconv>
#include <iostream>
#include <memory>
#include <ostream>

#include "imgui.h"
#include "assimp/scene.h"
#include "GLFW/glfw3.h"

void GUI_Mesh::Draw(GLFWwindow* window)
{
    ImGui::Begin("模型 Importer");
    //=========================================================================================================
    if (ImGui::Button("+", ImVec2(32.0f, 0.0f)))
    {
        const std::string path = 打开文件选择器(window); // 让用户挑一个模型文件
        if (!path.empty())
        {
            Model model;
            if (填充模型数据(path,model))
            {
                models.push_back(model);
            }
        }
    }
    //2-"-"：删除当前选中的子网格（和 "+" 排在同一行）
    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(32.0f, 0.0f)))
    {
        删除选中子网格();
    }
    //=========================================================================================================
    ImGui::SeparatorText(("模型列表:" + std::to_string(selectedModel)).c_str());

    int selected = 0;
    for (auto& model:models)
    {
        for (auto& mesh:model.subMesh)
        {
            //2-显示列表
            const std::string label = std::to_string(selected+1) +":"+model.name+":"+mesh.name ;
            if (ImGui::Selectable(label.c_str(), selectedModel == selected))
            {
                selectedModel = selected;
            }

            selected +=1;
        }
    }
    //=========================================================================================================
    ImGui::PushID("选中子网格");
    Other1::Model* 选中模型 = nullptr;
    Other1::Mesh* 选中网格 = nullptr;
    int 序号 = 0;
    for (auto& m : models)
    {
        for (auto& mesh : m.subMesh)
        {
            if (序号 == selectedModel)
            {
                选中模型 = &m;
                选中网格 = &mesh;
            }
            ++序号;
        }
    }
    if (选中网格 != nullptr)
    {
        ImGui::SeparatorText(("模型变换" + std::to_string(selectedModel)).c_str());
        ImGui::Text("名字: %s", 选中网格->name.c_str());
        ImGui::DragFloat3("位置", &选中网格->transform.position.x, 0.01f);
        ImGui::DragFloat3("旋转", &选中网格->transform.rotation.x, 0.5f);
        ImGui::DragFloat3("缩放", &选中网格->transform.scale.x, 0.01f);
    }
    ImGui::PopID();
    //=========================================================================================================
    if (选中网格 != nullptr)
    {
        ImGui::SeparatorText("模型调试信息");
        ImGui::Text("模型: %s", 选中模型->name.c_str());
        ImGui::Text("名字: %s", 选中网格->name.c_str());
        ImGui::Text("顶点数: %zu", 选中网格->vertices.size());
        ImGui::Text("索引数: %zu", 选中网格->indices.size());
        const Other1::Shader& shader = 选中网格->material.shader;
        ImGui::Text("顶点入口: %s", shader.vertName.c_str());
        ImGui::Text("片段入口: %s", shader.fragName.c_str());
        ImGui::Text("SPV 字节: 顶点 %zu / 片段 %zu", shader.spvVert.size(), shader.spvFrag.size());
        ImGui::Text("顶点输入项: %zu", shader.inputDescriptions.size());
    }
    //=========================================================================================================
    ImGui::End();
}

std::string GUI_Mesh::打开文件选择器(GLFWwindow* window)
{
    //1-接收所选文件路径的缓冲
    wchar_t fileName[MAX_PATH] = L"";

    //2-文件选择对话框的参数
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);                                       // 结构体大小
    ofn.hwndOwner = glfwGetWin32Window(window);                          // 挂在窗口上，不然对话框可能弹到窗口后面
    ofn.lpstrFilter = L"模型文件 (*.fbx)\0*.fbx\0所有文件 (*.*)\0*.*\0";   // 文件类型过滤：模型文件 / 所有文件
    ofn.lpstrFile = fileName;                                            // 指向接收路径的缓冲
    ofn.nMaxFile = MAX_PATH;                                             // 缓冲的最大字符数
    ofn.lpstrTitle = L"选择模型文件";                                      // 对话框标题
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR; // 文件必须存在且不改变工作目录

    //2-用户取消，返回空串
    if (GetOpenFileNameW(&ofn) == FALSE)
    {
        return std::string();
    }

    //3-所以这里必须转 UTF-8，转成 ANSI 反而不认识中文路径
    int size = WideCharToMultiByte(CP_UTF8, 0, fileName, -1, nullptr, 0, nullptr, nullptr); // 先算 UTF-8 所需字节数
    if (size <= 1) return std::string(); // 空路径则返回空串

    //4-按字节数分配 UTF-8 缓冲
    std::string path(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, fileName, -1, path.data(), size, nullptr, nullptr); // 真正转换为 UTF-8
    //5-返回所选路径
    return path;
}

void GUI_Mesh::删除选中子网格()
{
    if (selectedModel < 0) return;   // 一个都没选，没什么可删的

    //1-找到 selectedModel 落在哪个模型上
    int 序号 = 0;
    for (auto 模型 = models.begin(); 模型 != models.end(); ++模型)
    {
        const int 本模型数量 = static_cast<int>(模型->subMesh.size());
        if (selectedModel >= 序号 + 本模型数量)
        {
            序号 += 本模型数量;   // 选中的不在这个模型里，往下一个模型找
            continue;
        }

        // 选中的就是这个模型里的第 (selectedModel - 序号) 个子网格
        模型->subMesh.erase(模型->subMesh.begin() + (selectedModel - 序号));

        //2-子网格删空了就把这个模型也删掉，免得列表里留一个没有任何子网格的空壳
        if (模型->subMesh.empty())
        {
            models.erase(模型);
        }
        break;
    }

    //3-修正选中编号：删掉的位置现在正好是原来"下一项"的编号，所以 selectedModel 不用动；
    //  删的如果是最后一项，就往前退到新的末尾；一个都不剩了才置 -1
    int 总数 = 0;
    for (const Model& 模型 : models)
    {
        总数 += static_cast<int>(模型.subMesh.size());
    }
    selectedModel = (总数 == 0) ? -1 : std::min(selectedModel, 总数 - 1);
}

void GUI_Mesh::归一化模型(std::vector<Mesh>& meshes)
{
    constexpr float 目标尺寸 = 1.8f;   // 归一化后模型最长边的长度（世界单位）

    const Vertex* 首个顶点 = nullptr;
    for (const Mesh& mesh : meshes)
    {
        if (!mesh.vertices.empty())
        {
            首个顶点 = &mesh.vertices[0];
            break;
        }
    }
    if (首个顶点 == nullptr) return;

    glm::vec3 minBounds = 首个顶点->position;
    glm::vec3 maxBounds = minBounds;

    //2-包围盒跨所有子网格一起算
    for (const Mesh& mesh : meshes)
    {
        for (const Vertex& vertex : mesh.vertices)
        {
            minBounds = glm::min(minBounds, vertex.position);
            maxBounds = glm::max(maxBounds, vertex.position);
        }
    }

    const glm::vec3 center = (minBounds + maxBounds) * 0.5f;   // 包围盒中心
    const glm::vec3 size = maxBounds - minBounds;              // 包围盒尺寸
    const float maxSize = std::max(size.x, std::max(size.y, size.z)); // 最长边的长度

    const float normalizeScale = (maxSize > 0.0f) ? (目标尺寸 / maxSize) : 1.0f; // 缩放系数：把最长边缩到目标尺寸

    for (Mesh& mesh : meshes)
    {
        for (Vertex& vertex : mesh.vertices)
        {
            vertex.position = (vertex.position - center) * normalizeScale;
        }
    }
}

bool GUI_Mesh::填充模型数据(std::string path, Other1::Model& m)
{
    //1-Assimp 导入器
    std::shared_ptr<Assimp::Importer> importer = std::make_shared<Assimp::Importer>();
    const aiScene* scene = importer->ReadFile(path,
                                 aiProcess_Triangulate |    // 文件里的面可能不是三角形
                                 aiProcess_GenSmoothNormals |     // 没有法线时自动生成
                                 aiProcess_CalcTangentSpace |     // 关键：不打开的话 HasTangentsAndBitangents() 恒为 false，切线只能是假数据
                                 aiProcess_FlipUVs |              // FBX/OBJ 的 UV 原点在左下角，图片原点在左上角
                                 aiProcess_PreTransformVertices); // 默认不需要动画/节点树，直接烘成世界空间顶点
    //2-加载失败
    if (scene == nullptr|| (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0 || scene->mRootNode == nullptr)
    {
        return false;
    }
    else  //3-加载成功
    {
        Model model;
        uint32_t vertexIdBase = 0;   // 整个模型已累计的顶点数，用来算 vertexId

        /*动画*/for (unsigned int m = 0; m < scene->mNumAnimations; m++){}
        /*相机*/for (unsigned int m = 0; m < scene->mNumCameras; m++){}
        /*材质*/for (unsigned int m = 0; m < scene->mNumMaterials; m++){}
        /*灯光*/for (unsigned int m = 0; m < scene->mNumLights; m++){}
        /*网格*/for (unsigned int m = 0; m < scene->mNumMeshes; m++)
        {
            const aiMesh* srcMesh = scene->mMeshes[m];
            Mesh subMesh;
            //2-填充名字
            model.name = srcMesh->mName.length > 0 ? srcMesh->mName.C_Str() : "<unnamed>";
            subMesh.name = std::to_string(m);
            //3-填充顶点缓冲区
            for (unsigned int v = 0; v < srcMesh->mNumVertices; v++)
            {
                Vertex vertex;
                //位置
                vertex.position = glm::vec3(srcMesh->mVertices[v].x, srcMesh->mVertices[v].y, srcMesh->mVertices[v].z);
                //法线
                vertex.normal = srcMesh->HasNormals()//法线
                        ? glm::vec3(srcMesh->mNormals[v].x, srcMesh->mNormals[v].y, srcMesh->mNormals[v].z)
                        : glm::vec3(0.0f, 1.0f, 0.0f);
                //切线
                vertex.tangents = srcMesh->HasTangentsAndBitangents() //切线
                        ? glm::vec3(srcMesh->mTangents[v].x, srcMesh->mTangents[v].y, srcMesh->mTangents[v].z)
                        : glm::vec3(1.0f, 0.0f, 0.0f);

                //顶点色
                vertex.color = srcMesh->HasVertexColors(0) // 没有顶点色时用白色，颜色交给材质决定
                       ? glm::vec4(srcMesh->mColors[0][v].r, srcMesh->mColors[0][v].g, srcMesh->mColors[0][v].b, srcMesh->mColors[0][v].a)
                       : glm::vec4(1.0f);

                //UV0 ~ UV3：读之前必须先判断该通道有没有贴图坐标，没有就填 0
                vertex.uv0 = srcMesh->HasTextureCoords(0) ? glm::vec2(srcMesh->mTextureCoords[0][v].x, srcMesh->mTextureCoords[0][v].y) : glm::vec2(0.0f);
                vertex.uv1 = srcMesh->HasTextureCoords(1) ? glm::vec2(srcMesh->mTextureCoords[1][v].x, srcMesh->mTextureCoords[1][v].y) : glm::vec2(0.0f);
                vertex.uv2 = srcMesh->HasTextureCoords(2) ? glm::vec2(srcMesh->mTextureCoords[2][v].x, srcMesh->mTextureCoords[2][v].y) : glm::vec2(0.0f);
                vertex.uv3 = srcMesh->HasTextureCoords(3) ? glm::vec2(srcMesh->mTextureCoords[3][v].x, srcMesh->mTextureCoords[3][v].y) : glm::vec2(0.0f);

                //来源网格序号 / 该顶点在整个顶点缓冲里的下标
                vertex.meshIndex = m;
                vertex.vertexId = vertexIdBase + v;

                //存进这个子网格的顶点数组
                subMesh.vertices.push_back(vertex);
            }

            for (unsigned int f = 0; f < srcMesh->mNumFaces; f++)
            {
                const aiFace& face = srcMesh->mFaces[f];
                for (unsigned int i = 0; i < face.mNumIndices; i++)
                {
                    subMesh.indices.push_back(face.mIndices[i]);
                }
            }

            //下一个网格的顶点从当前累计数往后排
            vertexIdBase += srcMesh->mNumVertices;
            subMesh.indexCount = static_cast<uint32_t>(subMesh.indices.size());
            model.subMesh.push_back(subMesh);
        }
        /*蒙皮*/for (unsigned int m = 0; m < scene->mNumSkeletons; m++){}
        /*纹理*/for (unsigned int m = 0; m < scene->mNumTextures; m++){}

        //-后处理
        归一化模型(model.subMesh);
        m = model;
        return true;
    }
}

bool GUI_Mesh::返回选中对象(int& seleMeshID, Other1::Mesh*& m)
{
    seleMeshID = -1;
    m = nullptr;
    if (selectedModel < 0) return false;   // 一个都没选

    //2-顺着 models -> subMesh 数下去，找到编号等于 selectedModel 的那个
    int 序号 = 0;
    for (auto& model : models)
    {
        for (auto& mesh : model.subMesh)
        {
            if (序号 == selectedModel)
            {
                seleMeshID = 序号;
                m = &mesh;
                return true;
            }
            ++序号;
        }
    }
    return false;
}
