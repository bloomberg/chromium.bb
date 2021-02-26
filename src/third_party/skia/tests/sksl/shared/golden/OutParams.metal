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
    float3 h3;
    {
        h3 = float3(3.0);
    }


    float4 h4;
    {
        h4 = float4(4.0);
    }


    {
        h3.xz = float2(2.0);
    }


    {
        h4.zwxy = float4(4.0);
    }


    _out->sk_FragColor = float4(1.0, 2.0, h3.x, h4.x);
    float3x3 h3x3;
    {
        h3x3 = float3x3(3.0);
    }


    float4x4 h4x4;
    {
        h4x4 = float4x4(4.0);
    }


    {
        h3x3[1] = float3(3.0);
    }


    {
        h4x4[3].w = 1.0;
    }


    _out->sk_FragColor = float4(float2x2(2.0)[0][0], h3x3[0][0], h4x4[0][0], 1.0);
    int4 i4;
    {
        i4 = int4(4);
    }


    {
        i4.xyz = int3(3);
    }


    _out->sk_FragColor = float4(1.0, 2.0, 3.0, float(i4.x));
    float3 f3;
    {
        f3 = float3(3.0);
    }


    {
        f3.xy = float2(2.0);
    }


    _out->sk_FragColor = float4(1.0, 2.0, f3.x, 4.0);
    float2x2 f2x2;
    {
        f2x2 = float2x2(2.0);
    }


    {
        f2x2[0][0] = 1.0;
    }


    _out->sk_FragColor = float4(f2x2[0][0], float3x3(3.0)[0][0], float4x4(4.0)[0][0], 1.0);
    bool4 b4;
    {
        b4 = bool4(false);
    }


    {
        b4.xw = bool2(false);
    }


    _out->sk_FragColor = float4(1.0, bool2(false).x ? 1.0 : 0.0, bool3(true).x ? 1.0 : 0.0, b4.x ? 1.0 : 0.0);
    return *_out;
}
