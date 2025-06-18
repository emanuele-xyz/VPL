#include "Commons.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    float3 albedo = cb_object.albedo;
    float3 diffuse = albedo / PI; // Lambert diffuse BRDF
    
    float3 N = normalize(input.world_normal);
    float3 L = normalize(cb_light.world_position - input.world_position);
    float NdotL = max(dot(N, L), 0);
    
    float3 color = diffuse * cb_light.color * NdotL; // rendering equation
    
    return float4(color, 1.0);
}
