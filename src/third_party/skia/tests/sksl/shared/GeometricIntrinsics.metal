#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Uniforms {
    float4 colorGreen;
};
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};

fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    float _1_x = 1.0;
    _1_x = abs(1.0);
    _1_x = abs(_1_x - 2.0);
    _1_x = (_1_x * 2.0);
    _1_x = sign(_1_x);

    float2 _3_x = float2(1.0, 2.0);
    _3_x = float2(length(float2(1.0, 2.0)));
    _3_x = float2(distance(_3_x, float2(3.0, 4.0)));
    _3_x = float2(dot(_3_x, float2(3.0, 4.0)));
    _3_x = normalize(_3_x);

    _out.sk_FragColor = _uniforms.colorGreen;
    return _out;
}
