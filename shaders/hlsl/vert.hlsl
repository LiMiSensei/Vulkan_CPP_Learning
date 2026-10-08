struct VSInput
{
    [[vk::location(0)]]
    float3 inPosition  : POSITION;   // location 0
    float3 inColor     : TEXCOORD1;   // location 1
    float2 inUV0       : TEXCOORD2;   // location 2
    float3 inNormal    : TEXCOORD3;   // location 3
    uint   inMeshIndex : TEXCOORD4;   // location 4
    float2 inUV1       : TEXCOORD5;   // location 5
    float2 inUV2       : TEXCOORD6;   // location 6
    float2 inUV3       : TEXCOORD7;   // location 7
    uint   inVertexId  : TEXCOORD8;   // location 8
};

struct VSOutput
{
    float4 position : SV_Position;                 // = gl_Position
    float3 fragColor  : TEXCOORD0;
    float2 fragUV0    : TEXCOORD1;
    float3 fragNormal : TEXCOORD2;
    nointerpolation uint fragFace     : TEXCOORD3;  // = flat out uint
    float2 fragUV1    : TEXCOORD4;
    float2 fragUV2    : TEXCOORD5;
    float2 fragUV3    : TEXCOORD6;
    nointerpolation uint fragVertexId : TEXCOORD7;
};
struct UboCamera
{
    float4x4 view;
    float4x4 projection;
};
ConstantBuffer<UboCamera> ubo : register(b0);
struct PushModel
{
    float4x4 model;
    float4   normalCol0;
    float4   normalCol1;
    float4   normalCol2;
    int      debugMode;
    float    alpha;
};
[[vk::push_constant]] ConstantBuffer<PushModel> push;

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(push.model, float4(input.inPosition, 1.0));
    output.position = mul(ubo.projection, mul(ubo.view, worldPos));

    output.fragColor = input.inColor;
    output.fragUV0   = input.inUV0;
    output.fragUV1   = input.inUV1;
    output.fragUV2   = input.inUV2;
    output.fragUV3   = input.inUV3;
    float3 n = input.inNormal;
    output.fragNormal = push.normalCol0.xyz * n.x
                      + push.normalCol1.xyz * n.y
                      + push.normalCol2.xyz * n.z;

    output.fragFace     = input.inMeshIndex;
    output.fragVertexId = input.inVertexId;

    return output;
}