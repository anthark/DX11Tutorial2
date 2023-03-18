struct Light
{
    float4 pos;
    float4 color;
};

cbuffer SceneBuffer : register (b0)
{
    float4x4 vp;
    float4 cameraPos; // Camera position
    int4 lightCount; // x - light count (max 10), y - use normal maps, z - show normals instead of color, w - use culling
    int4 postProcess; // x - use sepia
    Light lights[10];
    float4 ambientColor;
};
