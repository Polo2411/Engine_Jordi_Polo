cbuffer Transforms : register(b0)
{
    float4x4 mvp;
};

struct VSOut
{
    float2 texCoord : TEXCOORD;
    float4 position : SV_POSITION;
};

VSOut main(float3 position : POSITION, float2 texCoord : TEXCOORD)
{
    VSOut o;
    o.position = mul(float4(position, 1.0f), mvp);
    o.texCoord = texCoord;
    return o;
}
