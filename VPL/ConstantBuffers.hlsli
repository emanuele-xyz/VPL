#ifndef __CONSTANT_BUFFERS__
#define __CONSTANT_BUFFERS__

#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SIGN_COS_WEIGHTED 1
#define LIGHT_TYPE_COS_WEIGHTED 2

struct SceneConstants
{
    matrix view;
    matrix projection;
    float3 world_eye;
    float particles_count;
    float far_plane;
    float _pad[3];
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
    float intensity;
    float3 normal;
    int type;
};

#endif
