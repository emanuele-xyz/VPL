#include "Commons.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    float3 albedo = cb_object.albedo;
    float3 diffuse = albedo / PI; // Lambert diffuse BRDF
    
    // compute cosine weight for VPL
    float weight = 0;
    {
        float3 minus_L = normalize(input.world_position - cb_light.world_position);
        float3 VPL_N = normalize(cb_light.normal);
        float v = max(dot(minus_L, VPL_N), 0);
        weight = sign(v); // TODO: maybe try cosine weight instead of sign weight
    }
    
    float3 N = normalize(input.world_normal);
    float3 L = normalize(cb_light.world_position - input.world_position);
    float NdotL = max(dot(N, L), 0);
    
    float3 color = diffuse * weight * cb_light.color * NdotL; // rendering equation
    
    return float4(color, 1.0);
}
