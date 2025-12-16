float4 main(float2 uv : TEXCOORD) : SV_TARGET
{
    return float4(frac(uv), 0.0, 1.0);
}
