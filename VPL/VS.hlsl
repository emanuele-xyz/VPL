#include "Commons.hlsli"

VSOutput main(VSInput input)
{
    VSOutput output;
    output.world_position = mul(cb_object.model, float4(input.local_position, 1.0)).xyz;
    output.clip_position = mul(cb_scene.projection, mul(cb_scene.view, float4(output.world_position, 1.0)));
    output.world_normal = mul(cb_object.normal, float4(input.local_normal, 0.0)).xyz;
    return output;
}
