#ifndef __COMMONS__
#define __COMMONS__

#include "ConstantBuffers.hlsli"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

cbuffer CBScene : register(b0)
{
    SceneConstants cb_scene;
};

cbuffer CBObject : register(b1)
{
    ObjectConstants cb_object;
};

#endif
