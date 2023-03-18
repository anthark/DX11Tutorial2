#include "SceneCB.h"

float3 CalculateColor(in float3 objColor, in float3 objNormal, in float3 pos, in float shine, in bool trans)
{
    float3 finalColor = float3(0, 0, 0);

    if (lightCount.z > 0)
    {
        return float3(objNormal * 0.5 + float3(0.5, 0.5, 0.5));
    }

    for (int i = 0; i < lightCount.x; i++)
    {
        float3 normal = objNormal;

        float3 lightDir = lights[i].pos.xyz - pos;
        float lightDist = length(lightDir);
        lightDir /= lightDist;

        float atten = clamp(1.0 / (lightDist * lightDist), 0, 1);

        if (trans && dot(lightDir, objNormal) < 0.0)
        {
            normal = -normal;
        }

        // Diffuse part
        finalColor += objColor * max(dot(lightDir, normal), 0) * atten * lights[i].color.xyz;

        float3 viewDir = normalize(cameraPos.xyz - pos);
        float3 reflectDir = reflect(-lightDir, normal);

        float spec = shine > 0 ? pow(max(dot(viewDir, reflectDir), 0.0), shine) : 0.0;

        // Specular part
        finalColor += objColor * 0.5 * spec * lights[i].color.xyz;
    }

    return finalColor;
}