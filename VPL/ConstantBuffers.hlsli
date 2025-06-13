#ifndef __CONSTANT_BUFFERS__
#define __CONSTANT_BUFFERS__

struct ObjectConstants
{
    matrix model;
    matrix normal;
    float3 albedo;
    float _pad[1];
};

#endif
