#include "Exercise6.hlsli"

// BasicMaterial slot 0 = base colour texture
Texture2D diffuseTex : register(t0);
SamplerState diffuseSamp : register(s0);

float4 main(float3 worldPos : POSITION, float3 normal : NORMAL, float2 coord : TEXCOORD) : SV_TARGET
{
    float3 Cd = diffuseColour.rgb;

    if (hasDiffuseTex)
        Cd *= diffuseTex.Sample(diffuseSamp, coord).rgb;

    float3 N = normalize(normal);

    // L is direction of light rays (pointing from light to surface)
    float3 Ldir = normalize(L);

    float dotNL = saturate(-dot(Ldir, N));

    float3 V = normalize(viewPos - worldPos);
    float3 R = reflect(Ldir, N);

    float dotVR = saturate(dot(V, R));

    float3 colour =
        Cd * (Kd * dotNL) * Lc +
        Cd * Ac +
        (Ks * pow(dotVR, shininess)) * Lc;

    return float4(colour, 1.0f);
}
