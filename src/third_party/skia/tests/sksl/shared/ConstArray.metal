#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};
struct Globals {
    const array<float, 4> test;
};
fragment Outputs fragmentMain(Inputs _in [[stage_in]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Globals _globals{array<float, 4>{0.0, 1.0, 0.0, 1.0}};
    (void)_globals;
    Outputs _out;
    (void)_out;
    _out.sk_FragColor = float4(_globals.test[0], _globals.test[1], _globals.test[2], _globals.test[3]);
    return _out;
}
