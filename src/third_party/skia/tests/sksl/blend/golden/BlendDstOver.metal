#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Inputs {
    float4 src;
    float4 dst;
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};


fragment Outputs fragmentMain(Inputs _in [[stage_in]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _outputStruct;
    thread Outputs* _out = &_outputStruct;
    float4 _0_blend_dst_over;
    {
        _0_blend_dst_over = (1.0 - _in.dst.w) * _in.src + _in.dst;
    }

    _out->sk_FragColor = _0_blend_dst_over;

    return *_out;
}
