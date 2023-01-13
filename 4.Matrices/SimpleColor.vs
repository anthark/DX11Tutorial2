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
    float4 color : COLOR;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    //result.pos = mul(modelMatrix, float4(vertex.pos, 1.0));
	result.pos = mul(vp, mul(model, float4(vertex.pos, 1.0)));
    result.color = vertex.color;

    return result;
}
