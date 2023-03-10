cbuffer SceneBuffer : register (b0)
{
    float4x4 vp;
};

cbuffer GeomBuffer : register (b1)
{
    float4x4 model;
};

struct VSInput
{
    float3 pos : POSITION;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 worldPos : POSITION;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float3 worldPos = mul(model, float4(vertex.pos, 1.0)).xyz;

    result.pos = mul(vp, float4(worldPos, 1.0));
    result.worldPos = worldPos;

    return result;
}
