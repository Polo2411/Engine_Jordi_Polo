#include "Exercise6.hlsli"

// BasicMaterial slot 0 = base colour texture
Texture2D diffuseTex : register(t0);
SamplerState diffuseSamp : register(s0);

static const float PI = 3.14159265359f;

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // Schlick approximation
    float t = pow(saturate(1.0f - cosTheta), 5.0f);
    return F0 + (1.0f - F0) * t;
}

float4 main(float3 worldPos : POSITION, float3 normal : NORMAL, float2 coord : TEXCOORD) : SV_TARGET
{
    // Cd (diffuse albedo)
    float3 Cd = diffuseColour.rgb;
    if (hasDiffuseTex != 0)
        Cd *= diffuseTex.Sample(diffuseSamp, coord).rgb;

    float3 N = normalize(normal);

    // L is ray direction (light -> surface). We want Ls = (surface -> light).
    float3 Ldir = normalize(L);
    float3 Ls = -Ldir;

    float NoL = saturate(dot(N, Ls));
    if (NoL <= 0.0f)
    {
        // No direct light, keep ambient only
        return float4(Cd * Ac, 1.0f);
    }

    float3 V = normalize(viewPos - worldPos);

    // Half-vector (for Fresnel)
    float3 H = normalize(Ls + V);
    float VoH = saturate(dot(V, H));

    // Fresnel term (RGB)
    float3 F = FresnelSchlick(VoH, saturate(specularColour));

    // Energy conservation split (constant, avoids breaking reciprocity)
    float kS = max(specularColour.r, max(specularColour.g, specularColour.b));
    float kD = saturate(1.0f - kS);

    // Diffuse BRDF (Lambert)
    float3 diffuseBRDF = (kD * Cd);

    // Normalized Phong specular BRDF:
    // BRDFspec = F * ((n+2)/(2π)) * (R·V)^n
    float3 R = reflect(-Ls, N); // reflect incoming (-Ls) around N
    float RoV = saturate(dot(R, V));
    float phongLobe = pow(RoV, shininess);

    float normPhong = (shininess + 2.0f) * 0.5f;
    float3 specularBRDF = F * (normPhong * phongLobe);

    // Rendering equation for 1 directional light: Lo = (fd + fs) * Li * NoL
    float3 direct = (diffuseBRDF + specularBRDF) * Lc * NoL;

    // Ambient (keep it simple like your previous shader)
    float3 ambient = Cd * Ac;

    return float4(direct + ambient, 1.0f);
}
