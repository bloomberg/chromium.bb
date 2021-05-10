#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Uniforms {
    float4 colorGreen;
    float4 colorRed;
};
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};


bool check(float2 v1, float2 v2, float2 v3, float3 v4, int2 v5, int2 v6, float2 v7, float2 v8, float4 v9, int2 v10, bool4 v11, float2 v12, float2 v13, float2 v14, bool2 v15, bool2 v16, bool3 v17) {
    return (((((((((((((((v1.x + v2.x) + v3.x) + v4.x) + float(v5.x)) + float(v6.x)) + v7.x) + v8.x) + v9.x) + float(v10.x)) + float(v11.x)) + v12.x) + v13.x) + v14.x) + float(v15.x)) + float(v16.x)) + float(v17.x) == 17.0;
}
fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    float4 v9 = float4(1.0, sqrt(2.0), float2(int2(3, 4)));
    _out.sk_FragColor = check(float2(1.0), float2(1.0, 2.0), float2(1.0), float3(float2(1.0), 1.0), int2(1), int2(float2(1.0, 2.0)), float2(int2(1, 2)), float2(int2(1)), v9, int2(3, 1), bool4(true, false, true, false), float2(1.0, 0.0), float2(0.0), float2(bool2(false)), bool2(true), bool2(float2(1.0)), bool3(true, bool2(int2(77)))) ? _uniforms.colorGreen : _uniforms.colorRed;
    return _out;
}
