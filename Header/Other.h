//
// Created by LiMi on 2026/9/30.
//

#ifndef VULKAN_CPP_LEARNING_OTHER_H
#define VULKAN_CPP_LEARNING_OTHER_H
#include "assimp/Vertex.h"


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>   // glm::inverseTranspose：算法线矩阵

//RenderState / VerInputDes 用到 VkFormat、VK_BLEND_FACTOR_* 等 Vulkan 符号，
//这里必须自带这个头，否则像 GUI_Hierarchy.h 这种第一个就 include Other.h 的文件会报"未声明的标识符"
#include <vulkan/vulkan_core.h>



namespace Other1
{
    //=====================================
    //渲染状态 每材质
    struct RenderState {
        //混合模式
        bool blendEnable = false;                                 // 混合开关
        int blendSrcFactor = VK_BLEND_FACTOR_SRC_ALPHA;           // 源混合因子
        int blendDstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // 目标混合因子
        int blendOp = VK_BLEND_OP_ADD;                            // 混合运算
        //剔除模式
        int cullMode = VK_CULL_MODE_BACK_BIT;                     // 剔除哪一面（None / Front / Back / Front and back）
        int frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;          // 正面绕序（Counter clockwise / Clockwise）
        //深度
        bool depthWriteEnable = true;                             // 深度写入开关（半透明材质应关掉，否则会挡住后面的物体）
        bool depthTestEnable = true;                              // 深度测试开关
        int depthCompareOp = VK_COMPARE_OP_LESS;                  // 深度比较模式
        //模板值
        bool stencilTestEnable = false;                           // 模板测试开关
        int stencilReference = 0;                                 // 模板参考值（模板缓冲被清成 0，比较用 EQUAL）
        int ColorMask;
    };
    //变换 GameObject&Camera
    struct Transform
    {
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};

        glm::mat4 toMatrix() const
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, position);
            model = glm::rotate(
                model,
                glm::radians(rotation.x),
                glm::vec3(1, 0, 0)
            );
            model = glm::rotate(
                model,
                glm::radians(rotation.y),
                glm::vec3(0, 1, 0)
            );
            model = glm::rotate(
                model,
                glm::radians(rotation.z),
                glm::vec3(0, 0, 1)
            );
            model = glm::scale(model, scale);
            return model;
        }
        glm::vec3 forward() const
        {
            glm::mat4 rotationMatrix(1.0f);
            // Yaw
            rotationMatrix = glm::rotate(
                rotationMatrix,
                glm::radians(rotation.y),
                glm::vec3(0, 1, 0)
            );
            // Pitch
            rotationMatrix = glm::rotate(
                rotationMatrix,
                glm::radians(rotation.x),
                glm::vec3(1, 0, 0)
            );
            return glm::normalize(
                glm::vec3(
                    rotationMatrix *
                    glm::vec4(0, 0, -1, 0)
                )
            );
        }
        glm::vec3 downward() const
        {
            glm::mat4 rotationMatrix(1.0f);
            // Yaw
            rotationMatrix = glm::rotate(
                rotationMatrix,
                glm::radians(rotation.y),
                glm::vec3(0, 1, 0)
            );
            // Pitch
            rotationMatrix = glm::rotate(
                rotationMatrix,
                glm::radians(rotation.x),
                glm::vec3(1, 0, 0)
            );
            // 和 forward() 共用同一套 YXZ 旋转，只是基准向量从"前"(0,0,-1) 换成"下"(0,-1,0)：
            // 物体正立时它的下方就是世界 -Y。w 给 0 才是方向向量——给 1 的话平移会跟着加进来
            return glm::normalize(
                glm::vec3(
                    rotationMatrix *
                    glm::vec4(0, -1, 0, 0)
                )
            );
        }
        glm::vec3 up() const
        {
            glm::mat4 rotationMatrix(1.0f);
            // Yaw
            rotationMatrix = glm::rotate(
                rotationMatrix,
                glm::radians(rotation.y),
                glm::vec3(0, 1, 0)
            );
            // Pitch
            rotationMatrix = glm::rotate(
                rotationMatrix,
                glm::radians(rotation.x),
                glm::vec3(1, 0, 0)
            );
            return glm::normalize(
                glm::vec3(
                    rotationMatrix *
                    glm::vec4(0, 1, 0, 0)
                )
            );
        }
        
    };
    //appdata输入
    struct VerInputDes
    {
        uint32_t location;
        VkFormat format;
        uint32_t offset;
    };
    //Shader
    struct Shader
    {
        std::vector<char> spvVert;//SPV着色代码
        std::vector<char> spvFrag;//SPV着色代码
        std::string vertName;     //顶点入口函数
        std::string fragName;   //片段入口函数
        //顶点输入
        std::vector<VerInputDes> inputDescriptions;
    };

    struct GUITest
    {
        //纹理忒图
        std::vector<std::tuple<std::string,std::string>> textures;
        //浮点，bool，int，
        std::vector<std::tuple<std::string, int>> ints;
        std::vector<std::tuple<std::string, float>> floats;
        std::vector<std::tuple<std::string, bool>> bools;
        //std::vector<std::tuple<std::string, glm::vec2>> vec2s;
        //std::vector<std::tuple<std::string, glm::vec3>> vec3s;
        std::vector<std::tuple<std::string, glm::vec4>> vec4s;
        //颜色
        std::vector<std::tuple<std::string, glm::vec4>> colors;
        //枚举int
        std::vector<std::tuple<std::string,std::vector<std::string>,int>> inenumsts;
        //slider滑动条
        std::vector<std::tuple<std::string,float,float,float>> sliders;
    };
    //材质球
    struct Material
    {
        Shader shader;
        RenderState state;                       // 这个模型自己的渲染状态（深度 / 模板 / 混合）
        GUITest guiTest;
    };

    //顶点缓冲
    struct  Vertex
    {
        glm::vec3 position;    // 位置
        glm::vec3 normal;     // 法线
        glm::vec3 tangents;    // 法线
        glm::vec4 color;       // 顶点色（Assimp 的 COLOR0；模型没有就填 MODEL_BASE_COLOR）
        glm::vec2 uv0;         // UV 通道 0（大多数模型只有这一套）
        glm::vec2 uv1;         // UV 通道 1（贴图 / 光照贴图等，没有就是 0）
        glm::vec2 uv2;         // UV 通道 2
        glm::vec2 uv3;         // UV 通道 3
        uint32_t meshIndex;    // 来源网格（mesh）序号，传给片元着色器
        uint32_t vertexId;     // 顶点 ID：该顶点在整个顶点缓冲里的下标

    };
    //子网格
    struct Mesh
    {
        std::string name;
        Transform transform;
        Material material;
        std::vector<Vertex> vertices;            // CPU 侧顶点（加载时已归一化到原点附近）
        std::vector<uint32_t> indices;           // 顶点索引

        //---- GPU 资源（由 TestRender 创建 / 销毁）----
        VkBuffer       vertexBuffer       = VK_NULL_HANDLE;  // 顶点缓冲
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;  // 顶点缓冲显存

        VkBuffer       indexBuffer        = VK_NULL_HANDLE;  // 索引缓冲
        VkDeviceMemory indexBufferMemory  = VK_NULL_HANDLE;  // 索引缓冲显存

        uint32_t       indexCount         = 0;               // 索引数量
        VkPipeline     pipeline           = VK_NULL_HANDLE;  // 材质管线；shader 为空时保持 VK_NULL_HANDLE，渲染时据此跳过

    };
    //=====================================
    struct Model
    {
        std::string name;
        std::vector<Mesh> subMesh;
        glm::mat4 modelMatrix = glm::mat4(1.0f);
    };
    struct Camera
    {
        std::string name;
        //1-变换组件
        Transform transform = Transform(glm::vec3(0.0f,0.0f,3.0f));
        //2-基础参数
        float fov = 60.0f;       // 焦距
        float nearPlane = 0.1f;  // 近裁剪面
        float farPlane = 1000.0f; // 远裁剪面
        float moveSpeed = 2.5f;  // 移动速度（单位 / 秒，WASD 平移和 Q / E 升降共用）
        glm::vec4 clearColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

        //3-矩阵
        glm::mat4 view = glm::mat4(1.0f);      // 视图矩阵
        glm::mat4 projection = glm::mat4(1.0f);// 投影矩阵
    };
    struct MainLight
    {
        std::string name;
        //1-变换组件
        Transform transform;
        //主灯光相关参数
        glm::vec4 color = glm::vec4(1.0f);
    };
    //=====================================
}

#endif //VULKAN_CPP_LEARNING_OTHER_H
