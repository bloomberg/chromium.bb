#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Uniforms {
    float unknownInput;
};
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};
fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    float4 color = float4(0.0);
    for (int counter = 0;counter < 10; ++counter) {
    }
    for (int counter = 0;counter < 10; ++counter) {
    }
    for (int counter = 0;counter < 10; ++counter) {
    }
    if (_uniforms.unknownInput == 1.0) color.y = 1.0;
    if (_uniforms.unknownInput == 2.0) ; else color.w = 1.0;
    _out.sk_FragColor = color;
    return _out;
}
