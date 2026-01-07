
cbuffer PerFrame : register(b1)
{
    float3 L; // Light direction (world space)
    float _pfPad0;
    float3 Lc; // Light colour
    float _pfPad1;
    float3 Ac; // Ambient colour
    float _pfPad2;
    float3 viewPos; // Camera position (world space)
    float _pfPad3;
};

cbuffer PerInstance : register(b2)
{
    float4x4 modelMat;
    float4x4 normalMat;

    float4 diffuseColour;
    float Kd;
    float Ks;
    float shininess;
    bool hasDiffuseTex;
    float3 _piPad0;
};
