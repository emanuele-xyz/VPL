#include "Commons.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    float3 albedo = cb_object.albedo;
    float3 diffuse = albedo / PI; // Lambert diffuse BRDF
    
    float3 N = normalize(input.world_normal);
    float3 L = normalize(cb_light.world_position - input.world_position);
    float NdotL = max(dot(N, L), 0);
    
    float weight = 1;
    if (cb_light.type == LIGHT_TYPE_POINT)
    {
        weight = 1;
    }
    else if (cb_light.type == LIGHT_TYPE_COS_WEIGHTED)
    {
        float3 light_n = normalize(cb_light.normal);
        weight = max(dot(light_n, -L), 0);
    }
    else if (cb_light.type == LIGHT_TYPE_SIGN_COS_WEIGHTED)
    {
        float3 light_n = normalize(cb_light.normal);
        weight = sign(max(dot(light_n, -L), 0));
    }
    
    float3 light = weight * cb_light.intensity * cb_light.color;
    
    float3 color = diffuse * light * NdotL; // rendering equation
    
    color /= cb_scene.particles_count; // abiding by Keller, each frame is weighted by the number of particles // TODO: we should enable this
    
    return float4(color, 1.0);
}
