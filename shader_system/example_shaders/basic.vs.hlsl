// basic.vs.hlsl – Simple passthrough vertex shader

struct VSInput {
    float3 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 normal   : NORMAL;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

cbuffer TransformCB : register(b0) {
    float4x4 worldViewProj;
    float4x4 world;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), worldViewProj);
    output.texCoord = input.texCoord;
    output.normal   = mul(input.normal, (float3x3)world);
    return output;
}
