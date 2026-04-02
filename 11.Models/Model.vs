#include "SceneCB.h"
#include "ModelCB.h"

struct VSInput
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 norm : NORMAL;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;
    result.pos = mul(vp, mul(trans, float4(vertex.pos, 1.0)));
    result.norm = mul(transNormal, float4(vertex.norm, 1.0));
    return result;
}
