#include "Commons.hlsli"

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.normal = input.normal;
    return output;
}
