#ifndef __COMMONS__
#define __COMMONS__

#include "ConstantBuffers.hlsli"

static const float PI = 3.14159265;

struct VSInput
{
    float3 local_position : POSITION;
    float3 local_normal : NORMAL;
};

struct VSOutput
{
    float4 clip_position : SV_Position;
    float3 world_normal : NORMAL;
    float3 world_position : POSITION;
};

cbuffer CBScene : register(b0)
{
    SceneConstants cb_scene;
};

cbuffer CBObject : register(b1)
{
    ObjectConstants cb_object;
};

cbuffer CBLight : register(b2)
{
    LightConstants cb_light;
};

cbuffer CBShadow : register(b3)
{
    ShadowConstants cb_shadow;
};

TextureCube cube_shadow_map : register(t0);

SamplerState shadow_sampler : register(s0);
SamplerState skybox_sampler : register(s1);

#endif
