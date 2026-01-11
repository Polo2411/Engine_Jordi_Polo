cbuffer PerFrame : register(b1)
{
    float3 L; // Light ray direction (world): from light to surface
    float _pfPad0;

    float3 Lc; // Light colour
    float _pfPad1;

    float3 Ac; // Ambient colour
    float _pfPad2;

    float3 viewPos; // Camera position (world)
    float _pfPad3;
};

cbuffer PerInstance : register(b2)
{
    float4x4 modelMat;
    float4x4 normalMat;

    float4 diffuseColour; // Cd (rgb) + alpha
    float3 specularColour; // F0 (rgb)
    float shininess; // n

    uint hasDiffuseTex; // match C++ BOOL (4 bytes)
    float3 _piPad0; // padding to 16-byte
};
