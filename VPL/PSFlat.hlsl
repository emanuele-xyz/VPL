#include "Commons.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    return float4(input.normal, 1); // TODO: temporary
}
