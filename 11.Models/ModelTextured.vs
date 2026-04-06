#include "SceneCB.h"
#include "ModelCB.h"

#ifdef ANIMATION

int JointIndex(int idx)
{
    int4 idxValues = jointIndices[idx / 4];

    switch (idx % 4)
    {
    case 0:
        return idxValues.x;
        break;
    case 1:
        return idxValues.y;
        break;
    case 2:
        return idxValues.z;
        break;
    case 3:
        return idxValues.w;
        break;
    }

    return 0;
}

#endif // ANIMATION

struct VSInput
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float2 uv: TEXCOORD;
#ifdef ANIMATION
    uint4 joints : JOINTS;
    float4 weights : WEIGHTS;
#endif // ANIMATION
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 norm : NORMAL;
    float2 uv: TEXCOORD;
};

VSOutput vs(VSInput vertex)
{
#ifdef ANIMATION
    // AAV TEMP
    int4 jointIdx = int4(
        JointIndex(vertex.joints.x),
        JointIndex(vertex.joints.y),
        JointIndex(vertex.joints.z),
        JointIndex(vertex.joints.w)
    );

    /*float4x4 _transform = nodeTransforms[jointIdx.x] * vertex.weights.x
        + nodeTransforms[jointIdx.y] * vertex.weights.y
        + nodeTransforms[jointIdx.z] * vertex.weights.z
        + nodeTransforms[jointIdx.w] * vertex.weights.w;
    float4x4 _transformNormals = nodeNormalTransforms[jointIdx.x] * vertex.weights.x
        + nodeNormalTransforms[jointIdx.y] * vertex.weights.y
        + nodeNormalTransforms[jointIdx.z] * vertex.weights.z
        + nodeNormalTransforms[jointIdx.w] * vertex.weights.w;*/

    float4x4 _transform = nodeTransforms[jointIdx.x];
    float4x4 _transformNormals = nodeNormalTransforms[jointIdx.x];
#else
    float4x4 _transform = trans;
    float4x4 _transformNormals = transNormal;
#endif // !ANIMATION

    VSOutput result;
    result.pos = mul(vp, mul(_transform, float4(vertex.pos, 1.0)));
    result.norm = mul(_transformNormals, float4(vertex.norm, 1.0));
    result.uv = vertex.uv;
    return result;
}
