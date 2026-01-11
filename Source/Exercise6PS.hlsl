#include "Exercise6.hlsli"

Texture2D diffuseTex : register(t0);
SamplerState diffuseSamp : register(s0);

float3 FresnelSchlick(float3 F0, float dotNL)
{
    float t = pow(saturate(1.0f - dotNL), 5.0f);
    return F0 + (1.0f - F0) * t;
}

float4 main(float3 worldPos : POSITION, float3 normal : NORMAL, float2 coord : TEXCOORD) : SV_TARGET
{
    float3 Cd = diffuseColour.rgb;
    if (hasDiffuseTex != 0)
        Cd *= diffuseTex.Sample(diffuseSamp, coord).rgb;

    float3 N = normalize(normal);

    // L is the light ray direction (light -> surface)
    float3 Ldir = normalize(L);

    // NoL with this convention uses -dot(L, N)
    float NoL = saturate(-dot(Ldir, N));
    if (NoL <= 0.0f)
        return float4(Cd * Ac, 1.0f);

    float3 V = normalize(viewPos - worldPos);

    // Phong reflection vector
    float3 R = reflect(Ldir, N);
    float RoV = saturate(dot(R, V));

    float3 F0 = saturate(specularColour);

    // Fresnel term
    float3 F = FresnelSchlick(F0, NoL);

    // Energy conservation: use max(F0) as a constant split
    float kS = max(max(F0.r, F0.g), F0.b);
    float kD = saturate(1.0f - kS);

    // Diffuse + normalized Phong specular (without PI divisions)
    float3 diffuseTerm = Cd * kD;
    float3 specularTerm = ((shininess + 2.0f) * 0.5f) * F * pow(RoV, shininess);

    float3 direct = (diffuseTerm + specularTerm) * Lc * NoL;
    float3 ambient = Cd * Ac;

    return float4(direct + ambient, 1.0f);
}
