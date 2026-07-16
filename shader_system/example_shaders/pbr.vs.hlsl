// pbr.vs.hlsl – PBR-ready vertex shader with variant defines
//
// Supported compile-time defines:
//   SKINNED   – enable skeletal animation (blend weights / indices)
//   INSTANCED – read per-instance world matrix from a structured buffer

struct VSInput {
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float4 tangent   : TANGENT;    // w encodes handedness
    float2 texCoord  : TEXCOORD0;

#if defined(SKINNED)
    float4 blendWeights : BLENDWEIGHT;
    uint4  blendIndices : BLENDINDICES;
#endif

#if defined(INSTANCED)
    uint   instanceId : SV_InstanceID;
#endif
};

struct VSOutput {
    float4 clipPos   : SV_POSITION;
    float2 texCoord  : TEXCOORD0;
    float3 worldPos  : TEXCOORD1;
    float3 worldNorm : TEXCOORD2;
    float3 worldTan  : TEXCOORD3;
    float3 worldBtan : TEXCOORD4;
};

cbuffer CameraCB : register(b0) {
    float4x4 viewProj;
    float3   cameraPos;
    float    _pad0;
};

cbuffer ObjectCB : register(b1) {
    float4x4 world;
};

#if defined(SKINNED)
cbuffer SkinCB : register(b2) {
    float4x4 boneMatrices[128];
};

float4x4 getSkinMatrix(float4 weights, uint4 indices) {
    return weights.x * boneMatrices[indices.x]
         + weights.y * boneMatrices[indices.y]
         + weights.z * boneMatrices[indices.z]
         + weights.w * boneMatrices[indices.w];
}
#endif

#if defined(INSTANCED)
StructuredBuffer<float4x4> instanceWorlds : register(t0);
#endif

VSOutput main(VSInput input) {
    VSOutput o;

    float4x4 localToWorld =
#if defined(INSTANCED)
        instanceWorlds[input.instanceId];
#else
        world;
#endif

#if defined(SKINNED)
    float4x4 skin = getSkinMatrix(input.blendWeights, input.blendIndices);
    float3 pos  = mul(float4(input.position, 1.0f), skin).xyz;
    float3 norm = mul(input.normal, (float3x3)skin);
    float3 tan  = mul(input.tangent.xyz, (float3x3)skin);
#else
    float3 pos  = input.position;
    float3 norm = input.normal;
    float3 tan  = input.tangent.xyz;
#endif

    float4 worldPos4 = mul(float4(pos, 1.0f), localToWorld);
    o.worldPos   = worldPos4.xyz;
    o.clipPos    = mul(worldPos4, viewProj);
    o.texCoord   = input.texCoord;

    float3x3 normalMat = (float3x3)localToWorld; // assumes uniform scale
    o.worldNorm  = normalize(mul(norm, normalMat));
    o.worldTan   = normalize(mul(tan,  normalMat));
    o.worldBtan  = cross(o.worldNorm, o.worldTan) * input.tangent.w;

    return o;
}
