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
    float far_plane; // TODO: this should go in MainLightConstants
    float _pad[3]; // TODO: remove padding
};

struct ObjectConstants
{
    matrix model;
    matrix normal;
    float3 albedo;
    float _pad;
};

/*
struct MainLightConstants
{
    float3 world_position;
    float intensity;
    float3 color;
    float far_plane;
};
*/

struct LightConstants // TODO: this should be renamed VirtualLightConstants
{
    float3 world_position;
    float radius;
    float3 color;
    float intensity;
    float3 normal;
    int type;
};

#endif
