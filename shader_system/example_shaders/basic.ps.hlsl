// basic.ps.hlsl – Simple diffuse pixel shader

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float3 normal   : TEXCOORD1;
};

Texture2D    albedoMap : register(t0);
SamplerState linearSampler : register(s0);

cbuffer LightCB : register(b1) {
    float3 lightDir;
    float  padding;
    float3 lightColor;
    float  ambientIntensity;
};

float4 main(PSInput input) : SV_Target {
    float3 n         = normalize(input.normal);
    float  diffuse   = max(dot(n, -normalize(lightDir)), 0.0f);
    float3 albedo    = albedoMap.Sample(linearSampler, input.texCoord).rgb;
    float3 ambient   = albedo * ambientIntensity;
    float3 color     = ambient + albedo * lightColor * diffuse;
    return float4(color, 1.0f);
}
