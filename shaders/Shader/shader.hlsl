#pragma vertex VSMain
#pragma fragment PSMain

struct VSInput
{
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float4 color     : COLOR;

    float2 inUV0 : TEXCOORD1;
    float2 inUV1 : TEXCOORD2;
    float2 inUV2 : TEXCOORD3;
    float2 inUV3 : TEXCOORD4;
};

struct VSOutput
{
    float4 position     : SV_Position;
    float3 fragNormal   : TEXCOORD0;
    float3 fragTangent  : TEXCOORD1;
    float3 fragColor    : TEXCOORD2;
    float2 fragUV0      : TEXCOORD3;
    float2 fragUV1      : TEXCOORD4;
    float2 fragUV2      : TEXCOORD5;
    float2 fragUV3      : TEXCOORD6;
};


VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.fragNormal = input.normal;
    output.fragTangent = input.tangent;
    output.fragColor = input.color.rgb;
    output.fragUV0 = input.inUV0;
    output.fragUV1 = input.inUV1;
    output.fragUV2 = input.inUV2;
    output.fragUV3 = input.inUV3;
    return output;
}

Texture2D albedoTexture : register(t1, space0);
SamplerState albedoSampler : register(s0, space0);


float4 PSMain(VSOutput input) : SV_Target
{
    return float4(1.0, 1.0, 1.0, 1);
}
