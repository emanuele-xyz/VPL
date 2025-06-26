#ifndef __CONSTANT_BUFFERS__
#define __CONSTANT_BUFFERS__

struct SceneConstants
{
    matrix view;
    matrix projection;
    float3 world_eye;
    float particles_count;
};

struct ObjectConstants
{
    matrix model;
    matrix normal;
    float3 albedo;
    float _pad;
};

struct LightConstants
{
    float3 world_position;
    float radius;
    float3 color;
    float _pad0; // TODO: int light_type (POINT, COS_WEIGHTED, SIGN_COS_WEIGHTED). Remember to #define int to be int32_t in main. If you do this, also unify Lit shaders
    float3 normal;
    float _pad1;
};

#endif
