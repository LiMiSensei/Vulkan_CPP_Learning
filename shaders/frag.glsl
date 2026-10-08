#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV0;
layout(location = 2) in vec3 fragNormal;      // 世界空间法线
layout(location = 3) flat in uint fragFace;   // mesh 序号
layout(location = 4) in vec2 fragUV1;
layout(location = 5) in vec2 fragUV2;
layout(location = 6) in vec2 fragUV3;
layout(location = 7) flat in uint fragVertexId;

// 反照率贴图（binding 1）。目前绑的是 1x1 白色占位，乘上去不影响观感；
// 换成真实贴图后这里立刻生效
layout(binding = 1) uniform sampler2D albedoTexture;

// 推送常量块（布局必须和 vert.glsl、以及 C++ 里的 PushModel 一致）
layout(push_constant) uniform PushModel {
    mat4 model;
    vec4 normalCol0;
    vec4 normalCol1;
    vec4 normalCol2;
    int debugMode;   // 0 = 正常光照，其余见下面的 switch（由 ImGui 里选，和 Hierarchy 的列表顺序一致）
    float alpha;     // 片元输出的 alpha（在 Inspector 面板里调，配合混合开关看半透明）
} push;

layout(location = 0) out vec4 outColor;

// 把 UV 直接当颜色显示：红 = u，绿 = v（能直观看出 UV 布局和接缝）
vec3 uvToColor(vec2 uv) {
    return vec3(uv, 0.0);
}

void main() {
    // face：判断当前片元在正面还是背面（光栅化阶段给出的 gl_FrontFacing）0 = 正面，1 = 背面
    uint face = gl_FrontFacing ? 0u : 1u;

    vec3 color;
    switch (push.debugMode) {
        case 1: color = uvToColor(fragUV0); break;
        case 2: color = uvToColor(fragUV1); break;
        case 3: color = uvToColor(fragUV2); break;
        case 4: color = uvToColor(fragUV3); break;
        case 5: color = fragColor; break;   // 顶点色
        case 6: {
            // 顶点 ID：低位当红、中间位当绿、高位当蓝，相邻顶点颜色差别很大，方便看顶点分布
            uint id = fragVertexId;
            color = vec3(float(id & 0xFFu), float((id >> 8) & 0xFFu), float((id >> 16) & 0xFFu)) / 255.0;
            break;
        }
        case 7: color = normalize(fragNormal) * 0.5 + 0.5; break;   // 法线（范围映射到 0~1）
        default: {
            // 简单的朗伯光照，再乘上反照率贴图
            vec3 normal = normalize(fragNormal);
            float diffuse = max(dot(normal, normalize(vec3(0.45, 0.8, 0.35))), 0.0);
            color = fragColor * texture(albedoTexture, fragUV0).rgb * (0.25 + 0.75 * diffuse);

            // 背面（关掉背面剔除或把相机移进模型内部时才可见）：压暗，用 mesh 序号做点区分
            if (face == 1u) {
                color *= 0.2 + 0.01 * float(fragFace % 8u);
            }
            break;
        }
    }

    outColor = vec4(color, push.alpha);
}
