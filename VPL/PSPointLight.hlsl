#include "Commons.hlsli"

struct PSOut
{
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

PSOut main(VSOutput input)
{
    PSOut output;
    output.depth = 1.0;
    output.color = cb_light.intensity * float4(cb_light.color, 1.0);
    
    float3 ro = cb_scene.world_eye; // ray origin
    float3 rd = normalize(input.world_position - cb_scene.world_eye); // ray direction
    float3 sc = cb_light.world_position; // sphere center
    float sr = cb_light.radius; // sphere radius
    
    float a = dot(rd, rd);
    float b = 2 * dot(ro, rd) - 2 * dot(rd, sc);
    float c = dot(ro, ro) + dot(sc, sc) - 2 * dot(ro, sc) - sr * sr;
    
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant >= 0)
    {
        float t = (-b - sqrt(discriminant)) / (2 * a);
        
        if (t >= 0)
        {
            float3 world_sphere_point = ro + t * rd;
            float4 clip_sphere_point = mul(cb_scene.projection, mul(cb_scene.view, float4(world_sphere_point, 1)));
            float4 ndc_sphere_point = clip_sphere_point.xyzw / clip_sphere_point.w; // perspective divide
         
            output.depth = ndc_sphere_point.z;
        }
    }
    
    return output;
}
