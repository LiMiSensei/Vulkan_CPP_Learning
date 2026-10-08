#version 450

// 顶点输入：位置、顶点色、UV0~UV3、法线、mesh 序号、顶点 ID
// （维度和顺序必须和 C++ 里 Vertex 的成员、以及管线里的 attributeDescriptions 一一对应）
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;      // mesh 序号
layout(location = 5) in vec2 inUV1;
layout(location = 6) in vec2 inUV2;
layout(location = 7) in vec2 inUV3;
layout(location = 8) in uint inVertexId;  // 顶点 ID

// 相机数据（所有模型共用）：视图矩阵 + 投影矩阵
layout(binding = 0) uniform UboCamera {
    mat4 view;
    mat4 projection;
} ubo;

// 每个模型各自的模型矩阵 / 法线矩阵 / 调试模式 / alpha（推送常量）
// 布局必须和 frag.glsl、以及 C++ 里的 PushModel 完全一致。
// 法线矩阵用 3 个 vec4 存三列，避免 mat3 在 std430 下「每列按 vec4 对齐」带来的歧义
layout(push_constant) uniform PushModel {
    mat4 model;
    vec4 normalCol0;
    vec4 normalCol1;
    vec4 normalCol2;
    int debugMode;
    float alpha;
} push;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV0;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) flat out uint fragFace;      // 整数类型必须用 flat，不做插值
layout(location = 4) out vec2 fragUV1;
layout(location = 5) out vec2 fragUV2;
layout(location = 6) out vec2 fragUV3;
layout(location = 7) flat out uint fragVertexId;

void main() {
    gl_Position = ubo.projection * ubo.view * push.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragUV0 = inUV0;
    fragUV1 = inUV1;
    fragUV2 = inUV2;
    fragUV3 = inUV3;
    // 法线用「模型矩阵左上 3x3 的逆转置」变换：分轴缩放时不能直接用法线乘模型矩阵
    mat3 normalMatrix = mat3(push.normalCol0.xyz, push.normalCol1.xyz, push.normalCol2.xyz);
    fragNormal = normalMatrix * inNormal;
    fragFace = inFace;
    fragVertexId = inVertexId;
}
