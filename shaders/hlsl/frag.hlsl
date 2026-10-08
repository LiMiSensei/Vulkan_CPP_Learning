
    #pragma vertex VSMain
    #pragma fragment VSMain
    struct VSInput
    {
        float3 position  : TEXCOORD0;
        float3 normal    : TEXCOORD1;
        float2 tangent   : TEXCOORD2;
        float4 color     : TEXCOORD3;

        float2 inUV0     : TEXCOORD4;
        float2 inUV1     : TEXCOORD5;
        float2 inUV2     : TEXCOORD6;
        float2 inUV3     : TEXCOORD7;
    };

    struct VSOutput
    {
        float4 position : SV_Position;
    };

    VSOutput VSMain(VSInput input)
    {
        VSOutput output;
        output.position = float4(input.position, 1.0);
        return output;
    }

    struct PSInput
    {
        float3 fragColor      : COLOR0;
        float2 fragUV0        : TEXCOORD0;
        float3 fragNormal     : TEXCOORD1;
        nointerpolation uint fragFace : TEXCOORD2;
        float2 fragUV1        : TEXCOORD3;
        float2 fragUV2        : TEXCOORD4;
        float2 fragUV3        : TEXCOORD5;
        nointerpolation uint fragVertexId : TEXCOORD6;
    };

    Texture2D albedoTexture : register(t1, space0);
    SamplerState albedoSampler : register(s0, space0);

    struct PushModel
    {
        float4x4 model;
        float4 normalCol0;
        float4 normalCol1;
        float4 normalCol2;
        int debugMode;
        float alpha;
    };

    [[vk::push_constant]]
    ConstantBuffer<PushModel> push;

    float3 uvToColor(float2 uv)
    {
        return float3(uv, 0.0);
    }
    float4 PSMain(PSInput input) : SV_Target
    {
        float3 color = float3(1.0, 1.0, 1.0);
        return float4(color, push.alpha);
    }
