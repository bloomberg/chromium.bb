#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};
struct Globals {
    texture2d<float> test2D;
    sampler test2DSmplr;
    texture2d<float> test2DRect;
    sampler test2DRectSmplr;
};

fragment Outputs fragmentMain(Inputs _in [[stage_in]], texture2d<float> test2D[[texture(0)]], sampler test2DSmplr[[sampler(0)]], texture2d<float> test2DRect[[texture(1)]], sampler test2DRectSmplr[[sampler(1)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Globals globalStruct{test2D, test2DSmplr, test2DRect, test2DRectSmplr};
    thread Globals* _globals = &globalStruct;
    (void)_globals;
    Outputs _outputStruct;
    thread Outputs* _out = &_outputStruct;
    float3 tmpCoord0;
    _out->sk_FragColor = _globals->test2D.sample(_globals->test2DSmplr, float2(0.5));
    _out->sk_FragColor = _globals->test2DRect.sample(_globals->test2DRectSmplr, float2(0.5));
    _out->sk_FragColor = _globals->test2DRect.sample(_globals->test2DRectSmplr, (tmpCoord0 = float3(0.5), tmpCoord0.xy / tmpCoord0.z));
    return *_out;
}
