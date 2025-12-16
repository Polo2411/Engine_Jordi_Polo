cbuffer Transform : register(b0)
{
    float4x4 mvp;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOut main(float3 pos : POSITION, float2 uv : TEXCOORD)
{
    VSOut o;
    o.pos = mul(float4(pos, 1.0), mvp);
    o.uv = uv;
    return o;
}
