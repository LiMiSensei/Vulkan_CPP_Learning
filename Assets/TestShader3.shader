Properties
{
    //todo:纹理还没做
    _albedoTexture("",Texture2D) = White {}
    _Test1("int",Int) = 0
    _Test2("float",Float) = 1
    _Test3("bool",Bool) = 1
    _Test4("vector",Vector) = (1,1,1,1)
    _Test5("color",Color) = (1,1,1,1)
    _Test6("range",Range(0,1)) = 1
}
Render
{
    //todo:RenderStare还没做
    Blend One Zero
    ZTest On
    ZWrite On
    Cull Off
}
HLSLPROGRAM
Pass
{
    #pragma vertex VSMain
    #pragma fragment PSMain

    #include <UniformBuffer>

    struct PushConstantData
    {
        float4x4 model;
    };
    [[vk::push_constant]] ConstantBuffer<PushConstantData> pushConstant;
    //todo：纹理导入还没做
    Texture2D albedoTexture : register(t1, space0);
    SamplerState albedoSampler : register(s0, space0);

    struct VSInput
    {
        float3 position  : POSITION;
        float3 normal    : NORMAL;
        float3 tangent   : TANGENT;
        float4 color     : COLOR;
        float2 inUV2     : TEXCOORD3;
        float2 inUV3     : TEXCOORD4;
        float2 inUV0     : TEXCOORD1;
        float2 inUV1     : TEXCOORD2;
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
        float3 fragCameraPos: TEXCOORD7;
        float2 screenUV     : TEXCOORD8;
    };
    VSOutput VSMain(VSInput input)
    {
        VSOutput output;
        const float4 positionWS = mul(pushConstant.model, float4(input.position, 1.0));
        output.position = mul(_MATRIX_P, mul(_MATRIX_V, positionWS));
        output.fragNormal = mul((float3x3)pushConstant.model, input.normal);
        output.fragTangent = input.tangent;
        output.fragColor = input.color.rgb;
        output.fragUV0 = input.inUV0;
        output.fragUV1 = input.inUV1;
        output.fragUV2 = input.inUV2;
        output.fragUV3 = input.inUV3;
        output.fragCameraPos = _WorldSpaceCameraPos.xyz;
        float3 ndc = output.position.xyz / output.position.w;
        float2 screenUV = ndc.xy * 0.5 + 0.5;
        output.screenUV = float2(screenUV.x, 1.0 - screenUV.y);
        return output;
    }

    float4 PSMain(VSOutput input) : SV_Target
    {
        const float3 光方向 = normalize(_MainLightDirection.xyz);
        const float3 基础色 = float3(0.8, 0.8, 0.8);
        const float3 环境光 = float3(0.15, 0.15, 0.15);
        const float3 法线 = normalize(input.fragNormal);
        const float 漫反射强度 = saturate(dot(法线, 光方向));
        //return float4(基础色 * 漫反射强度 * _MainLightColor.xyz + 环境光, 1.0);
        //float4 time = cos(_Time.y);
        return float4(input.fragNormal,1.0);
    }
}
ENDHLSL
