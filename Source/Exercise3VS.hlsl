cbuffer Transforms : register(b0)
{
    float4x4 mvp; // model * view * projection (ya transpuesta en CPU)
};

float4 main(float3 pos : MY_POSITION) : SV_POSITION
{
    // Pre-multiply vector por la matriz MVP
    return mul(float4(pos, 1.0f), mvp);
}
