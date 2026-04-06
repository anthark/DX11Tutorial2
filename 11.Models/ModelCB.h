cbuffer MeshBuffer : register (b1)
{
    float4x4 trans;
    float4x4 transNormal;
};

#ifdef ANIMATION

#define MAX_NODES 200

cbuffer ModelBuffer : register (b2)
{
    float4x4 nodeTransforms[MAX_NODES];
    float4x4 nodeNormalTransforms[MAX_NODES];
    int4 jointIndices[MAX_NODES / 4];
};

#endif // ANIMATION