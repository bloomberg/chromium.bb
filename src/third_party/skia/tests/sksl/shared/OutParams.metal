#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Uniforms {
    float4 colorGreen;
    float4 colorRed;
    float4 colorWhite;
};
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};



fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    float h;
    h = _uniforms.colorWhite.x;

    float2 h2;
    h2 = float2(_uniforms.colorWhite.y);

    float3 h3;
    h3 = float3(_uniforms.colorWhite.z);

    float4 h4;
    h4 = float4(_uniforms.colorWhite.w);

    h3.y = _uniforms.colorWhite.x;

    h3.xz = float2(_uniforms.colorWhite.y);

    h4.zwxy = float4(_uniforms.colorWhite.w);

    float2x2 h2x2;
    h2x2 = float2x2(_uniforms.colorWhite.x);

    float3x3 h3x3;
    h3x3 = float3x3(_uniforms.colorWhite.y);

    float4x4 h4x4;
    h4x4 = float4x4(_uniforms.colorWhite.z);

    h3x3[1] = float3(_uniforms.colorWhite.z);

    h4x4[3].w = _uniforms.colorWhite.x;

    h2x2[0].x = _uniforms.colorWhite.x;

    int i;
    i = int(_uniforms.colorWhite.x);

    int2 i2;
    i2 = int2(int(_uniforms.colorWhite.y));

    int3 i3;
    i3 = int3(int(_uniforms.colorWhite.z));

    int4 i4;
    i4 = int4(int(_uniforms.colorWhite.w));

    i4.xyz = int3(int(_uniforms.colorWhite.z));

    i2.y = int(_uniforms.colorWhite.x);

    float f;
    f = _uniforms.colorWhite.x;

    float2 f2;
    f2 = float2(_uniforms.colorWhite.y);

    float3 f3;
    f3 = float3(_uniforms.colorWhite.z);

    float4 f4;
    f4 = float4(_uniforms.colorWhite.w);

    f3.xy = float2(_uniforms.colorWhite.y);

    f2.x = _uniforms.colorWhite.x;

    float2x2 f2x2;
    f2x2 = float2x2(_uniforms.colorWhite.x);

    float3x3 f3x3;
    f3x3 = float3x3(_uniforms.colorWhite.y);

    float4x4 f4x4;
    f4x4 = float4x4(_uniforms.colorWhite.z);

    f2x2[0].x = _uniforms.colorWhite.x;

    bool b;
    b = bool(_uniforms.colorWhite.x);

    bool2 b2;
    b2 = bool2(bool(_uniforms.colorWhite.y));

    bool3 b3;
    b3 = bool3(bool(_uniforms.colorWhite.z));

    bool4 b4;
    b4 = bool4(bool(_uniforms.colorWhite.w));

    b4.xw = bool2(bool(_uniforms.colorWhite.y));

    b3.z = bool(_uniforms.colorWhite.x);

    bool ok = true;
    ok = 1.0 == (((((h * h2.x) * h3.x) * h4.x) * h2x2[0].x) * h3x3[0].x) * h4x4[0].x;
    ok = ok && 1.0 == (((((f * f2.x) * f3.x) * f4.x) * f2x2[0].x) * f3x3[0].x) * f4x4[0].x;
    ok = ok && 1 == ((i * i2.x) * i3.x) * i4.x;
    ok = ok && (((b && b2.x) && b3.x) && b4.x);
    _out.sk_FragColor = ok ? _uniforms.colorGreen : _uniforms.colorRed;
    return _out;
}
