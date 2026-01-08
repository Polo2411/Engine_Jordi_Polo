#include "Exercise6.hlsli"

cbuffer MVP : register(b0)
{
    float4x4 mvp;
};

struct VSOut
{
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 position : SV_POSITION;
};

VSOut main(float3 position : POSITION, float2 texCoord : TEXCOORD, float3 normal : NORMAL)
{
    VSOut o;

    float4 world = mul(float4(position, 1.0f), modelMat);
    o.worldPos = world.xyz;

    // normalMat is expected to be inverse-transpose(modelMat) (uploaded already)
    o.normal = mul(normal, (float3x3) normalMat);

    o.texCoord = texCoord;
    o.position = mul(float4(position, 1.0f), mvp);

    return o;
}
