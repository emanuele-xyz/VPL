#include "Commons.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    return float4(cube_shadow_map.Sample(skybox_sampler, input.world_position).xxx, 1);
}
