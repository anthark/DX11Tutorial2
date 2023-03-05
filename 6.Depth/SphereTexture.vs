cbuffer SceneBuffer : register (b0)
{
    float4x4 vp;
    float4 cameraPos;
};

cbuffer GeomBuffer : register (b1)
{
    float4x4 model;
    float4 size; // x - size of sphere
};

struct VSInput
{
    float3 pos : POSITION;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 localPos : POSITION1;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float3 pos = cameraPos.xyz + vertex.pos * size.x;

    result.pos = mul(vp, mul(model, float4(pos, 1.0)));
    result.pos.z = 0;
    result.localPos = vertex.pos;

    return result;
}
