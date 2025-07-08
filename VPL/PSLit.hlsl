#include "Commons.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    float3 albedo = cb_object.albedo;
    float3 diffuse = albedo / PI; // Lambert diffuse BRDF
    
    float3 N = normalize(input.world_normal);
    float3 L = normalize(cb_light.world_position - input.world_position);
    float NdotL = max(dot(N, L), 0);
    
    float3 light = cb_light.intensity * cb_light.color;
    
    float light_weight = 1;
    if (cb_light.type == LIGHT_TYPE_POINT)
    {
        light_weight = 1;
    }
    else if (cb_light.type == LIGHT_TYPE_COS_WEIGHTED)
    {
        float3 light_n = normalize(cb_light.normal);
        light_weight = max(dot(light_n, -L), 0);
    }
    else if (cb_light.type == LIGHT_TYPE_SIGN_COS_WEIGHTED)
    {
        float3 light_n = normalize(cb_light.normal);
        light_weight = sign(max(dot(light_n, -L), 0));
    }
    
    light *= light_weight; // weight light based on light type
    
    float shadow = 0;
    {
        // offsets used for PCF sampling
        float3 offsets[PCF_MAX_SAMPLES] =
        {
            float3(+0, +0, +0),
            float3(+1, +1, +1), float3(+1, -1, +1), float3(-1, -1, +1), float3(-1, +1, +1),
            float3(+1, +1, -1), float3(+1, -1, -1), float3(-1, -1, -1), float3(-1, +1, -1),
            float3(+1, +1, +0), float3(+1, -1, +0), float3(-1, -1, +0), float3(-1, +1, +0),
            float3(+1, +0, +1), float3(-1, +0, +1), float3(+1, +0, -1), float3(-1, +0, -1),
            float3(+0, +1, +1), float3(+0, -1, +1), float3(+0, -1, -1), float3(+0, +1, -1),
        };
        
        float3 v = input.world_position - cb_light.world_position;
        float distance = length(v); // world space distance between the light and the fragment
        
        for (int i = 0; i < cb_shadow.pcf_samples; i++)
        {
            // closest world space distance from the light, along v's direction
            float sampled_distance = cube_shadow_map.Sample(shadow_sampler, v + offsets[i] * cb_shadow.offset_scale).r;
            sampled_distance *= cb_shadow.far_plane; // undo [0;1] mapping
            if (distance - cb_shadow.bias > sampled_distance)
            {
                shadow += 1; // count the number of samples that pass the test
            }
        }
        shadow /= float(cb_shadow.pcf_samples); // normalize the number of samples that passed the test by the number of total samples
    }
    
    light = (1 - shadow) * light; // weight light using the shadow factor
    
    float3 color = diffuse * light * NdotL; // rendering equation
    color /= cb_scene.particles_count; // abiding by Keller, each frame is weighted by the number of particles
    
    return float4(color, 1.0);
}
