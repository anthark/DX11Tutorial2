struct VSInput
{
    unsigned int vertexId : SV_VertexID;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    float4 pos = float4(0,0,0,0);
    switch (vertex.vertexId)
    {
        case 0:
            pos = float4(-1,1,0,1);
            break;
        case 1:
            pos = float4(3,1,0,1);
            break;
        case 2:
            pos = float4(-1,-3,0,1);
            break;
    }

    result.pos = pos;
    result.uv = float2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);

    return result;
}
