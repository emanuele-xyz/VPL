#include "Commons.hlsli"

float4 main(VSOutput input) : SV_TARGET
{
    return float4(cb_object.albedo, 1);
}
