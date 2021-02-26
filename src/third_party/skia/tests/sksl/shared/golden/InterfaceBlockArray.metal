#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};
struct testBlock {
    float x;
} test[2];
struct Globals {
    constant testBlock* test;
};fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant testBlock[2]& test [[buffer(-1)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Globals globalStruct{&test};
    thread Globals* _globals = &globalStruct;
    (void)_globals;
    Outputs _outputStruct;
    thread Outputs* _out = &_outputStruct;
    _out->sk_FragColor = float4(_uniforms.test[1].x);
    return *_out;
}
