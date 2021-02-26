#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};
fragment Outputs fragmentMain(Inputs _in [[stage_in]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _outputStruct;
    thread Outputs* _out = &_outputStruct;
    if (sqrt(2.0) > 5.0) {
        _out->sk_FragColor = float4(0.75);
    } else {
        discard_fragment();
    }
    int i = 0;
    while (i < 10) {
        _out->sk_FragColor *= 0.5;
        i++;
    }
    do {
        _out->sk_FragColor += 0.25;
    } while (_out->sk_FragColor.x < 0.75);
    for (int i = 0;i < 10; i++) {
        if (i % 2 == 1) break; else continue;
    }
    return;
    return *_out;
}
