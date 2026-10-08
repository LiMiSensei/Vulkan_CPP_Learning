//
// Created by LiMi on 2026/10/3.
//

// Windows 文件选择器（GetOpenFileNameW）要用到 windows.h / commdlg.h：
// NOMINMAX 必须在 windows.h 之前定义，否则它的 min/max 宏会跟 std::min/max 冲突


#include "GUI_ShaderSystem.h"

#include <algorithm>  // std::find_if（在共享片段表里按名字查）
#include <cctype>     // std::isdigit（读 TEXCOORD 后面的通道号要用）
#include <charconv>
#include <iostream>
#include <memory>
#include <ostream>

#include "imgui.h"
#include "assimp/scene.h"
#include "GLFW/glfw3.h"
#include <fstream>    // ifstream 读文件
#include <iostream>   // cerr 报错
#include <sstream>    // ostringstream 把文件内容整段读进来

#include "imgui.h"
#include "GLFW/glfw3native.h"   // glfwGetWin32Window

// dxcapi.h 自己不带 COM 声明，它要求调用方已经拿到 IUnknown / IID_PPV_ARGS。
// 而这个头文件开头定义了 WIN32_LEAN_AND_MEAN，windows.h 就不会再捎带 objbase.h，
// 所以这里必须显式补一个（必须在 dxcapi.h 之前）
#include <objbase.h>
#include <dxcapi.h>   // DXC：运行时把 HLSL 编译成 SPIR-V（头文件在 Vulkan SDK 的 Include/dxc 里）



GUI_ShaderSystem::GUI_ShaderSystem(GUI_Mesh& model):model_gui(model)
{

}

void GUI_ShaderSystem::Draw(GLFWwindow* window)
{
    ImGui::Begin("材质 Importer");
    //===================================================================================================
    //1-打开文件选择器，返回文本
    if (ImGui::Button("+", ImVec2(32.0f, 0.0f)))
    {
        //-1弹出选择器挑 .shader，把文本整段读回来
        std::string text = 读取原文本(window);
        std::string text2 = 去除文本注释(text);
        if (!text.empty())
        {
            Other1::Material mat;
            //1-读取顶点着色器片段着色器
            auto shader = 读取顶点和片段着色器(text2);
            if (shader.spvVert.empty()||shader.spvFrag.empty())
            {
                // 编译失败要收尾，否则 ImGui 窗口栈不平衡，下一帧会崩（Release 下没有断言保护）
                ImGui::End();
                return;
            }
            //2-读取面板
            auto gui = 读取GUI面板(text2);
            //3-读取渲染状态
            auto state = 解析渲染状态(text2);
            //4-填充上
            mat.guiTest = gui;
            mat.shader = shader;
            mat.state = state;
            materials.push_back(mat);
        }
    }
    //2-"-"：删除当前选中的材质
    ImGui::SameLine();
    if (ImGui::Button("-", ImVec2(32.0f, 0.0f)))
    {
        if (0 <= selseID && selseID < materials.size())
        {
            materials.erase(materials.begin() + selseID);
            selseID = -1; // 删掉后就没有选中项了
        }
    }
    //2.5-"应用"：把选中的材质赋给模型列表里选中的那个子网格
    ImGui::SameLine();
    if (ImGui::Button("应用"))
    {
        if (0 <= selseID && selseID < materials.size())
        {
            //selectedModel 是"跨所有模型的子网格"统一编号（见 GUI_Model::Draw 的计数方式），
            //所以按同样的顺序遍历，才能定位是哪个模型的哪个子网格
            int 序号 = 0;
            for (auto& model : model_gui.models)
            {
                for (auto& mesh : model.subMesh)
                {
                    if (序号 == model_gui.selectedModel)
                    {
                        mesh.material = materials[selseID];
                    }
                    ++序号;
                }
            }
        }
    }
    //===================================================================================================
    ImGui::SeparatorText(("着色器列表:"));
    //3-材质列表
    for (auto i = 0;i<materials.size();i++)
    {
        const std::string 标签 = "材质 " + std::to_string(i);
        if (ImGui::Selectable(标签.c_str(), selseID == i))
        {
            selseID = i;
        }
    }
    //===================================================================================================
    ImGui::SeparatorText(("着色器参数:"));
    //显示选中的Material的GUI面板
    if (-1<selseID&& selseID<materials.size())
    {
        auto& ui = materials[selseID].guiTest; // 用引用，面板上的改动能写回材质
        //生产GUI：遍历 guiTest 的每一类，按类型摆对应的控件
        for (auto& [名字, 值] : ui.textures)              // 纹理（先只显示名字）
        {
            ImGui::Text("%s: %s", 名字.c_str(), 值.c_str());
        }
        for (auto& [名字, 值] : ui.ints)                  // 整数
        {
            ImGui::DragInt(名字.c_str(), &值);
        }
        for (auto& [名字, 值] : ui.floats)                // 浮点
        {
            ImGui::DragFloat(名字.c_str(), &值);
        }
        for (auto& [名字, 值] : ui.bools)                 // 开关
        {
            ImGui::Checkbox(名字.c_str(), &值);
        }
        for (auto& [名字, 值] : ui.vec4s)                 // 向量
        {
            ImGui::DragFloat4(名字.c_str(), &值.x);
        }
        for (auto& [名字, 值] : ui.colors)                // 颜色
        {
            ImGui::ColorEdit4(名字.c_str(), &值.x);
        }
        for (auto& [名字, 选项列表, 选中项] : ui.inenumsts) // 枚举下拉
        {
            std::vector<const char*> 选项;                 // Combo 要的是 const char* 数组
            for (auto& 选项文本 : 选项列表) 选项.push_back(选项文本.c_str());
            ImGui::Combo(名字.c_str(), &选中项, 选项.data(), static_cast<int>(选项.size()));
        }
        for (auto& [名字, 最小, 最大, 值] : ui.sliders)     // 滑动条
        {
            ImGui::SliderFloat(名字.c_str(), &值, 最小, 最大);
        }
    }
    ImGui::End();
}

//用win窗口读取文本文件
std::string GUI_ShaderSystem::读取原文本(GLFWwindow* window)
{
    //1-接收所选文件路径的缓冲
    wchar_t fileName[MAX_PATH] = L"";

    //2-文件选择对话框的参数
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);                                              // 结构体大小
    ofn.hwndOwner = glfwGetWin32Window(window);                                 // 挂在窗口上，不然对话框可能弹到窗口后面
    ofn.lpstrFilter = L"Shader 文件 (*.shader)\0*.shader\0所有文件 (*.*)\0*.*\0"; // 文件类型过滤：只挑 .shader
    ofn.lpstrFile = fileName;                                                   // 指向接收路径的缓冲
    ofn.nMaxFile = MAX_PATH;                                                    // 缓冲的最大字符数
    ofn.lpstrTitle = L"选择 .shader 文件";                                        // 对话框标题
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;         // 文件必须存在且不改变工作目录

    //3-用户取消，返回空串
    if (GetOpenFileNameW(&ofn) == FALSE)
    {
        return std::string();
    }

    //4-以二进制整段读出（ifstream 有 wchar_t* 重载，能直接吃宽字符路径）
    std::ifstream 文件(fileName, std::ios::binary);
    if (!文件)
    {
        std::cerr << "读取 .shader 文件失败" << std::endl;
        return std::string();
    }
    std::ostringstream 缓冲;
    缓冲 << 文件.rdbuf();

    //5-文件已保证是 UTF-8，直接整段返回
    return  缓冲.str();
}

std::string GUI_ShaderSystem::去除文本注释(std::string text)
{
    //5-逐行去掉 // 及其后面的注释
    std::string 文本 = text;
    {
        std::istringstream 输入(文本);
        std::ostringstream 输出;
        std::string 行;
        bool 首行 = true;
        while (std::getline(输入, 行))
        {
            const size_t 注释 = 行.find("//");
            if (注释 != std::string::npos)
            {
                行.erase(注释); // 砍掉 // 和它后面的所有字符
            }
            if (!首行) 输出 << '\n'; // 保持原来的行结构，不额外多出空行
            输出 << 行;
            首行 = false;
        }
        文本 = 输出.str();
    }
    return 文本;
}

//读取GUI面板
Other1::GUITest GUI_ShaderSystem::读取GUI面板(const std::string text)
{
    Other1::GUITest gui;

    //1-取出 Properties{} 里面的内容（已逐行去掉行首空格）
    const std::string t = 读取花括号内容("Properties", text);

    //2-两个小工具：去掉字符串首尾空白、把 "(1,1,1,1)" 读成 vec4
    auto 去空白 = [](std::string s)
    {
        const size_t 首 = s.find_first_not_of(" \t\r\n");
        if (首 == std::string::npos) return std::string();
        const size_t 尾 = s.find_last_not_of(" \t\r\n");
        return s.substr(首, 尾 - 首 + 1);
    };
    auto 读vec4 = [](std::string s)
    {
        for (char& c : s) if (c == '(' || c == ')' || c == ',') c = ' '; // 括号逗号换成空格，好让流直接读数字
        std::istringstream 输入(s);
        glm::vec4 v(0.0f);
        输入 >> v.x >> v.y >> v.z >> v.w;
        return v;
    };

    //3-逐行解析，格式固定为：_名字("显示名",类型) = 默认值
    std::istringstream 输入(t);
    std::string 行;
    while (std::getline(输入, 行))
    {
        const size_t 左括号 = 行.find('(');            // 名字后面那个 (
        const size_t 逗号   = 行.find(',', 左括号);    // 显示名和类型之间的 ,
        const size_t 右括号 = 行.find(')', 逗号);      // 类型的 )
        const size_t 等号   = 行.find('=', 右括号);    // 默认值前的 =
        if (左括号 == std::string::npos || 逗号 == std::string::npos ||
            右括号 == std::string::npos || 等号 == std::string::npos)
        {
            continue; // 这行不完整（比如 _Test6 少了个 )），先跳过
        }

        //4-逗号左边=显示名（去掉引号），逗号右边=类型，等号右边=默认值
        std::string 名字 = 去空白(行.substr(左括号 + 1, 逗号 - 左括号 - 1));
        if (名字.size() >= 2 && 名字.front() == '"' && 名字.back() == '"')
        {
            名字 = 名字.substr(1, 名字.size() - 2); // 去掉包裹显示的引号
        }
        const std::string 类型 = 去空白(行.substr(逗号 + 1, 右括号 - 逗号 - 1));
        const std::string 值   = 去空白(行.substr(等号 + 1));

        //5-按类型放进 GUITest 对应的容器
        if (类型 == "Int")         gui.ints.push_back({名字, std::stoi(值)});
        else if (类型 == "Float")  gui.floats.push_back({名字, std::stof(值)});
        else if (类型 == "Bool")   gui.bools.push_back({名字, 值 != "0"}); // 等号右边不是 0 就为 true
        else if (类型 == "Vector") gui.vec4s.push_back({名字, 读vec4(值)});
        else if (类型 == "Color")  gui.colors.push_back({名字, 读vec4(值)});
        // Range / Texture2D 等先不动，等确定写法再接
    }
    调试GUI(gui);
    return gui;
}

//解析渲染状态
Other1::RenderState GUI_ShaderSystem::解析渲染状态(const std::string text)
{
    Other1::RenderState renderState;
    //读取Render{}里面的内容

    std::string t = 读取花括号内容("Render",text);
   // std::cout <<"###############解析渲染状态############\n"<< t << std::endl;
    //1-Blend One Zero

    //2-ZTest On

    //3-ZWrite On

    //4-Cull Off
    return renderState;
}

//给顶点输入结构体的每个属性前面补上 [[vk::location(n)]]
//先定位 POSITION 所在的结构体，只改这一段：
//  POSITION 占 0，NORMAL/TANGENT/COLOR 按出现顺序从 1 依次排（计数 = 它们出现的个数）
//  TEXCOORDn 的位置 = 计数 + n（例：三者都在时计数=3，TEXCOORD1→4、TEXCOORD2→5、TEXCOORD3→6、TEXCOORD4→7）
std::string GUI_ShaderSystem::修改顶点输入名称(std::string text)
{
    //1-定位 POSITION，再往前回溯到它所在的那个 struct
    //  （全大写查找，不会误命中 VSOutput 里的 SV_Position 或成员名 position）
    const size_t 位置 = text.find("POSITION");
    if (位置 == std::string::npos) return text;             // 没有顶点输入语义，原样返回

    const size_t 结构体 = text.rfind("struct", 位置);
    if (结构体 == std::string::npos) return text;

    //2-找到这个 struct 的配对 '}'，只在这一对花括号之间动手
    const size_t 左 = text.find('{', 结构体);
    if (左 == std::string::npos || 左 > 位置) return text;

    int 层级 = 0;
    size_t 右 = std::string::npos;
    for (size_t i = 左; i < text.size(); ++i)
    {
        if (text[i] == '{') ++层级;
        else if (text[i] == '}' && --层级 == 0) { 右 = i; break; }
    }
    if (右 == std::string::npos || 右 <= 左 + 1) return text;

    const std::string 结构体内容 = text.substr(左 + 1, 右 - 左 - 1);

    //3-取一行 ":" 后面的语义名（去掉空白和分号），如 "POSITION" / "TEXCOORD3"；不是属性行就返回空串
    auto 取语义 = [](const std::string& 行)
    {
        const size_t 冒号 = 行.find(':');
        if (冒号 == std::string::npos) return std::string();
        const size_t 首 = 行.find_first_not_of(" \t\r;", 冒号 + 1);
        if (首 == std::string::npos) return std::string();
        const size_t 尾 = 行.find_last_not_of(" \t\r;");
        return 行.substr(首, 尾 - 首 + 1);
    };

    //4-第一遍：数出 NORMAL/TANGENT/COLOR 出现了几个，TEXCOORD 的编号要从这个计数往后接
    int 计数 = 1;
    {
        std::istringstream 输入(结构体内容);
        std::string 行;
        while (std::getline(输入, 行))
        {
            const std::string 语义 = 取语义(行);
            if (语义 == "NORMAL" || 语义 == "TANGENT" || 语义 == "COLOR") 计数++;
        }
    }

    //5-第二遍：逐行插注解。非 TEXCOORD 属性按出现顺序 0、1、2… 编号；TEXCOORDn 直接算成 计数 + n
    std::istringstream 输入(结构体内容);
    std::ostringstream 输出;
    std::string 行;
    int 序号 = 0;
    while (std::getline(输入, 行))
    {
        const std::string 语义 = 取语义(行);
        if (!语义.empty())
        {
            int 位置值 = 序号;
            if (语义.rfind("TEXCOORD", 0) == 0) // 以 TEXCOORD 开头
            {
                const std::string 编号 = 语义.substr(8); // "TEXCOORD" 正好 8 个字符
                if (!编号.empty()) 位置值 = 计数 + std::stoi(编号);
            }
            else
            {
                ++序号;
            }
            const size_t 首 = 行.find_first_not_of(" \t"); // 保留原有缩进，注解插在类型前面
            if (首 != std::string::npos)
                行.insert(首, "[[vk::location(" + std::to_string(位置值) + ")]]");
        }
        输出 << 行 << '\n';
    }

    //6-getline 吃掉了最后一行的行尾换行，原文若没有就补回来，免得把 '}' 挤到同一行
    std::string 改好的 = 输出.str();
    if (text[右 - 1] != '\n') 改好的.pop_back();

    //7-把改好的结构体拼回原文本，结构体外的东西一律不动
    return text.substr(0, 左 + 1) + 改好的 + text.substr(右);
}


std::vector<Other1::VerInputDes> GUI_ShaderSystem::读取顶点输入数据(std::string text)
{
    std::vector<Other1::VerInputDes> 结果;

    //语义 → Vertex 里对应成员的字节偏移（Assimp 载入时按这个结构体摆放顶点）
    auto 取偏移 = [](const std::string& 语义) -> uint32_t
    {
        if (语义 == "POSITION")  return offsetof(Other1::Vertex, position);
        if (语义 == "NORMAL")    return offsetof(Other1::Vertex, normal);
        if (语义 == "TANGENT")   return offsetof(Other1::Vertex, tangents); // Vertex 里叫 tangents
        if (语义 == "COLOR")     return offsetof(Other1::Vertex, color);
        if (语义 == "TEXCOORD0") return offsetof(Other1::Vertex, uv0);
        if (语义 == "TEXCOORD1") return offsetof(Other1::Vertex, uv1);
        if (语义 == "TEXCOORD2") return offsetof(Other1::Vertex, uv2);
        if (语义 == "TEXCOORD3") return offsetof(Other1::Vertex, uv3);
        return UINT32_MAX; // 不认识的语义，外层跳过
    };

    //类型 → VkFormat（目前只处理 float 系列，跟 Vertex 里的字段对得上）
    auto 取格式 = [](const std::string& 类型) -> VkFormat
    {
        if (类型 == "float")  return VK_FORMAT_R32_SFLOAT;
        if (类型 == "float2") return VK_FORMAT_R32G32_SFLOAT;
        if (类型 == "float3") return VK_FORMAT_R32G32B32_SFLOAT;
        if (类型 == "float4") return VK_FORMAT_R32G32B32A32_SFLOAT;
        return VK_FORMAT_UNDEFINED;
    };

    //从 start 起吃掉一段标识符（字母/数字/下划线），返回结束位置
    auto 标识符尾 = [](const std::string& 行, size_t start)
    {
        while (start < 行.size() &&
               (std::isalnum(static_cast<unsigned char>(行[start])) || 行[start] == '_'))
            ++start;
        return start;
    };

    //逐行找带 [[vk::location(]] 的属性行（VSOutput 没有这个注解，不会被读到）
    std::istringstream 输入(text);
    std::string 行;
    while (std::getline(输入, 行))
    {
        const size_t 注解 = 行.find("[[vk::location(");
        if (注解 == std::string::npos) continue;

        //1-location：注解括号里的数字
        const size_t 左括号 = 注解 + 15; // "[[vk::location(" 共 15 个字符
        const size_t 右括号 = 行.find(')', 左括号);
        if (右括号 == std::string::npos) continue;
        const uint32_t location = static_cast<uint32_t>(std::stoi(行.substr(左括号, 右括号 - 左括号)));

        //2-类型：注解 "]]" 之后那个词（如 float3）
        const size_t 类型起 = 行.find_first_not_of(" \t", 右括号 + 3); // 跳过 ")]" 和空白
        if (类型起 == std::string::npos) continue;
        const std::string 类型 = 行.substr(类型起, 标识符尾(行, 类型起) - 类型起);

        //3-语义：":" 后面那个词（如 POSITION / TEXCOORD1）
        const size_t 冒号 = 行.find(':', 类型起);
        if (冒号 == std::string::npos) continue;
        const size_t 语义起 = 行.find_first_not_of(" \t", 冒号 + 1);
        if (语义起 == std::string::npos) continue;
        const std::string 语义 = 行.substr(语义起, 标识符尾(行, 语义起) - 语义起);

        //4-填一条描述；语义不认识就跳过
        const uint32_t offset = 取偏移(语义);
        if (offset == UINT32_MAX) continue;

        Other1::VerInputDes d;
        d.location = location;
        d.format   = 取格式(类型);
        d.offset   = offset;
        结果.push_back(d);

        std::cout <<"###############读取顶点输入数据############\n"
        << "location: " << d.location << " format: " << d.format << " offset: " << d.offset << std::endl;
    }

    return 结果;
}

//读取顶点着色器
Other1::Shader GUI_ShaderSystem::读取顶点和片段着色器(std::string text)
{
    Other1::Shader shader;
    //1-取出 Pass{} 里面的内容
    std::string t   = 读取花括号内容("Pass",text);
    //2-v2f进行修改
    std::vector<Other1::VerInputDes> inputs;

    const std::string t2   = 修改顶点输入名称(t);
    shader.inputDescriptions = 读取顶点输入数据(t2);
    shader.vertName   = 读取着色主函数名字("vertex",t2);
    shader.fragName = 读取着色主函数名字("fragment",t2);
    shader.spvVert = 编译着色器(t2, shader.vertName, true);
    shader.spvFrag = 编译着色器(t2, shader.fragName, false);
    //std::cout <<"=============修改顶点输入名称:=============\n"<< t2 << std::endl;

    return shader;
}

//通用函数
std::string GUI_ShaderSystem::读取花括号内容(std::string beginning, std::string text)
{
    //1-找到 beginning 之后的第一个 '{'，它就是块内容的起点
    size_t 起点 = text.find(beginning);
    if (起点 == std::string::npos) return std::string();

    起点 = text.find('{', 起点 + beginning.size());
    if (起点 == std::string::npos) return std::string();

    //2-从 '{' 往后数括号层级，找到配对的 '}'（内容里可能有 {} 这种嵌套）
    int 层级 = 0;
    size_t 终点 = std::string::npos;
    for (size_t i = 起点; i < text.size(); ++i)
    {
        if (text[i] == '{')
        {
            ++层级;
        }
        else if (text[i] == '}')
        {
            if (--层级 == 0)
            {
                终点 = i;
                break;
            }
        }
    }
    if (终点 == std::string::npos) return std::string();
    //3-取两个括号之间的文本，逐行去掉行首空白
    std::istringstream 输入(text.substr(起点 + 1, 终点 - 起点 - 1));
    std::ostringstream 输出;
    std::string 行;
    while (std::getline(输入, 行))
    {
        const size_t 首 = 行.find_first_not_of(" \t\r"); // 行首的空格/制表符/回车
        行 = (首 == std::string::npos) ? std::string() : 行.substr(首);
        输出 << 行 << '\n';
    }
    //4-去掉首尾多余的空行
    std::string 结果 = 输出.str();
    const size_t 首 = 结果.find_first_not_of("\n");
    if (首 == std::string::npos) return std::string();
    const size_t 尾 = 结果.find_last_not_of("\n");
    结果 = 结果.substr(首, 尾 - 首 + 1);
    return 结果;
}

std::string GUI_ShaderSystem::读取着色主函数名字(std::string beginning, std::string text)
{
    //1-拼出要匹配的 pragma 关键字，如 "#pragma vertex"
    const std::string 关键字 = "#pragma " + beginning;

    size_t 起点 = text.find(关键字);
    if (起点 == std::string::npos) return std::string();

    起点 += 关键字.size(); // 跳到关键字后面，从这里开始就是入口名

    //2-一直取到本行末尾（回车、换行都算行尾）
    size_t 终点 = text.find_first_of("\r\n", 起点);
    if (终点 == std::string::npos) 终点 = text.size();

    const std::string 内容 = text.substr(起点, 终点 - 起点);

    //3-去掉两边的空白，比如 "#pragma vertex  VSMain" 中间多打的空格
    const size_t 首 = 内容.find_first_not_of(" \t");
    if (首 == std::string::npos) return std::string();
    const size_t 尾 = 内容.find_last_not_of(" \t");

    return 内容.substr(首, 尾 - 首 + 1);
}


//展开自定义 include：把 "#include <名字>" 或 "#include "名字"" 整行换成共享片段表里的正文。
//为什么不交给 DXC：它的 #include 只认磁盘上的真实文件，而这里要的是"引用内置片段"，
//所以只能在送进编译器之前自己展开。
//名字查不到就返回空串——让编译这一步直接失败，而不是把 #include 原样丢给 DXC 再报一堆看不懂的错
std::string GUI_ShaderSystem::展开共享片段(const std::string& text)
{
    std::istringstream 输入(text);
    std::ostringstream 输出;
    std::string 行;
    while (std::getline(输入, 行))
    {
        //1-只看以 #include 开头的行。行首的空格不能省：
        //  Pass 内容被 读取花括号内容 削过缩进，但传进这里的不一定是那段（比如整份文本），找一下更稳
        const size_t 行首 = 行.find_first_not_of(" \t");
        if (行首 == std::string::npos || 行.compare(行首, 8, "#include") != 0)
        {
            输出 << 行 << '\n';
            continue;
        }

        //2-取出 <> 或 "" 之间的名字。"是取 <名字>，取到 > 为止
        const size_t 名字起 = 行.find_first_of("<\"", 行首 + 8);
        const size_t 名字尾 = (名字起 == std::string::npos)
                              ? std::string::npos
                              : 行.find_first_of(">\"", 名字起 + 1);
        if (名字尾 == std::string::npos)
        {
            std::cerr << "include 写法不对(要写成 #include <名字> 或 #include \"名字\"): " << 行 << std::endl;
            return {};
        }
        const std::string 名字 = 行.substr(名字起 + 1, 名字尾 - 名字起 - 1);

        //3-查表，命中就把整行换成片段正文
        const auto 命中 = std::find_if(共享片段.begin(), 共享片段.end(),
            [&名字](const std::pair<std::string, std::string>& 条) { return 条.first == 名字; });
        if (命中 == 共享片段.end())
        {
            std::cerr << "找不到共享片段: " << 名字 << std::endl;
            return {};
        }
        输出 << 命中->second << '\n';
    }
    return 输出.str();
}

//把一段 HLSL 文本编译成 SPIR-V 字节（用 Vulkan SDK 自带的 DXC）
//  入口名：由 #pragma vertex / #pragma fragment 读出来的函数名，如 VSMain
//  是顶点：true 按 vs_6_0 编，false 按 ps_6_0 编
std::vector<char> GUI_ShaderSystem::编译着色器(std::string text, std::string 入口名, bool 是顶点)
{
    //0-先把自定义 include 展开。必须放在最前面：下面 DxcBuffer 的 Ptr 直接指向 text.data()，
    //  从这里往后就不能再改动 text 了（一旦扩容或重新赋值，data() 失效，DXC 读到的是野内存）
    text = 展开共享片段(text);
    if (text.empty()) return {};   // 展开失败（名字写错等），具体错误上面已经打到控制台了

    //1-拿到 dxc 的两个接口：工具（做 blob/编码）和编译器本体
    IDxcUtils*     工具   = nullptr;
    IDxcCompiler3* 编译器 = nullptr;
    if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&工具)))) return {};
    if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&编译器))))
    {
        工具->Release();
        return {};
    }

    //2-把源码包成 dxc 能吃的缓冲。文本已保证是 UTF-8，编码就直接给 DXC_CP_UTF8
    DxcBuffer 源码 = {};
    源码.Ptr      = text.data();
    源码.Size     = text.size();
    源码.Encoding = DXC_CP_UTF8;

    //3-编译参数：目标 profile + 入口函数名 + 输出 SPIR-V，
    const std::wstring 目标 = 是顶点 ? L"vs_6_0" : L"ps_6_0";
    const std::wstring 入口(入口名.begin(), 入口名.end()); // 入口名都是英文，直接逐字符转宽字符
    const wchar_t* 参数[] = {
        L"-T", 目标.c_str(),
        L"-E", 入口.c_str(),
        L"-spirv",
        L"-fspv-entrypoint-name=main",
    };

    IDxcResult* 结果 = nullptr;
    编译器->Compile(&源码, 参数, _countof(参数), nullptr, IID_PPV_ARGS(&结果));

    //4-统一收尾，免得下面每个 return 前都写一遍 Release
    auto 收尾 = [&]
    {
        if (结果) 结果->Release();
        编译器->Release();
        工具->Release();
    };

    if (!结果)
    {
        std::cerr << "编译着色器失败: DXC 没有返回结果" << std::endl;
        收尾();
        return {};
    }

    //5-编译出错时，把 dxc 的报错原样打到控制台，方便定位（比如某个类型没定义）
    HRESULT 状态 = S_OK;
    结果->GetStatus(&状态);
    if (FAILED(状态))
    {
        IDxcBlobEncoding* 报错 = nullptr;
        if (SUCCEEDED(结果->GetErrorBuffer(&报错)) && 报错)
        {
            std::cerr << "编译着色器失败(" << 入口名 << "):\n"
                      << std::string(static_cast<const char*>(报错->GetBufferPointer()), 报错->GetBufferSize())
                      << std::endl;
            报错->Release();
        }
        收尾();
        return {};
    }

    //6-取出 DXC_OUT_OBJECT（就是编好的 SPIR-V），拷进 vector 返回
    IDxcBlob* spv = nullptr;
    if (FAILED(结果->GetResult(&spv)) || spv == nullptr)
    {
        std::cerr << "编译着色器失败: 取不到 SPIR-V 输出" << std::endl;
        收尾();
        return {};
    }

    const char* 首地址 = static_cast<const char*>(spv->GetBufferPointer());
    std::vector<char> 字节(首地址, 首地址 + spv->GetBufferSize());

    spv->Release();
    收尾();

    std::cout << (是顶点 ? "顶点编译成功" : "片段编译成功") << std::endl;
    return 字节;
}

void GUI_ShaderSystem::添加到Material(int seleID, std::vector<Other1::Model> materials)
{

}


void GUI_ShaderSystem::调试GUI(Other1::GUITest gui)
{
    for (auto [name, value] : gui.ints)
    {
        std::cout <<"int类型:"<< name  <<":" <<value<<  std::endl;
    }
    for (auto [name, value] : gui.floats)
    {
        std::cout <<"floats类型:"<< name  <<":" <<value<<  std::endl;
    }

    for (auto [name, value] : gui.bools)
    {
        std::cout <<"bools类型:"<< name  <<":" <<value<<  std::endl;
    }
    for (auto [name, value] : gui.vec4s)
    {
        std::cout <<"vec4s类型:"<< name  <<":" <<"("<<value.x<<","<<value.y<<","<<value.z<<","<<value.w<<")"<<  std::endl;
    }
    for (auto [name, value] : gui.colors)
    {
        std::cout <<"colors类型:"<< name  <<":" <<"("<<value.x<<","<<value.y<<","<<value.z<<","<<value.w<<")"<<  std::endl;
    }

}

