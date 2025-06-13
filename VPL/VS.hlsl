#include "Commons.hlsli"

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(cb_object.model, float4(input.position, 1.0));
    output.normal = mul(cb_object.normal, float4(input.normal, 0.0)).xyz;
    return output;
}
