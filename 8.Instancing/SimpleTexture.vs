#include "SceneCB.h"

struct GeomBuffer
{
    float4x4 model;
    float4x4 norm;
    float4 shineSpeedTexIdNM; // x - shininess, y - rotation speed, z - texture id, w - normal map presence
    float4 angle; // x - current angle
};

cbuffer GeomBufferInst : register (b1)
{
    GeomBuffer geomBuffer[100];
};

struct VSInput
{
    float3 pos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;

    unsigned int instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float4 worldPos : POSITION;
    float3 tang : TANGENT;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;

    nointerpolation unsigned int instanceId : SV_InstanceID;
};

VSOutput vs(VSInput vertex)
{
    VSOutput result;

    unsigned int idx = vertex.instanceId;

    float4 worldPos = mul(geomBuffer[idx].model, float4(vertex.pos, 1.0));

    result.pos = mul(vp, worldPos);
    result.worldPos = worldPos;
    result.uv = vertex.uv;
    result.tang = mul(geomBuffer[idx].norm, float4(vertex.tang, 0)).xyz;
    result.norm = mul(geomBuffer[idx].norm, float4(vertex.norm, 0)).xyz;
    result.instanceId = vertex.instanceId;

    return result;
}
