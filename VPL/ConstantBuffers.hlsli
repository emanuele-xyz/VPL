#ifndef __CONSTANT_BUFFERS__
#define __CONSTANT_BUFFERS__

struct SceneConstants
{
    matrix view;
    matrix projection;
    float3 world_eye;
    float _pad; // TODO: particles_count
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
    float _pad; // TODO: is_hemisphere
    // TODO: float3 normal
    // TODO: float pad
};

#endif
