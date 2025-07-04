#include "Commons.hlsli"

struct PSOut
{
    float4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

PSOut main(VSOutput input)
{
    float d = length(input.world_position - cb_light.world_position); // world space distance between fragment and light
    d = d / cb_scene.far_plane; // map the distance to the [0;1] range
    
    PSOut output;
    output.color = float4(0, 0, 0, 1);
    output.depth = d; // use the scaled distance as depth
    
    return output;
}
