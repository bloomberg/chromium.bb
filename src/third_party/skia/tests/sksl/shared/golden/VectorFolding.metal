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
    {
        _out->sk_FragColor.x = 0.5;
        _out->sk_FragColor = float4(6.0, 7.0, 9.0, 11.0);
        _out->sk_FragColor = float4(7.0, 9.0, 9.0, 9.0);
        _out->sk_FragColor = float4(2.0, 4.0, 6.0, 8.0);
        _out->sk_FragColor = float4(12.0, 6.0, 4.0, 3.0);
        _out->sk_FragColor.x = 6.0;
        _out->sk_FragColor.x = 1.0;
        _out->sk_FragColor.x = -2.0;
        _out->sk_FragColor.x = 3.0;
        _out->sk_FragColor.x = 4.0;
        _out->sk_FragColor.x = -5.0;
        _out->sk_FragColor.x = 6.0;
        _out->sk_FragColor.x = 7.0;
        _out->sk_FragColor.x = -8.0;
        _out->sk_FragColor.x = 9.0;
        _out->sk_FragColor.x = -10.0;
        _out->sk_FragColor = float4(sqrt(1.0));
        _out->sk_FragColor = float4(sqrt(2.0));
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(sqrt(6.0));
        _out->sk_FragColor = float4(sqrt(7.0));
        _out->sk_FragColor = float4(sqrt(8.0));
        _out->sk_FragColor = float4(sqrt(9.0));
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(sqrt(12.0));
        _out->sk_FragColor = float4(sqrt(13.0));
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(sqrt(16.0));
        _out->sk_FragColor = float4(sqrt(17.0));
        _out->sk_FragColor = float4(0.0);
        _out->sk_FragColor = float4(sqrt(19.0));
        _out->sk_FragColor = float4(sqrt(19.5));
        _out->sk_FragColor = float4(sqrt(20.0));
        _out->sk_FragColor = float4(sqrt(21.0));
        _out->sk_FragColor = float4(sqrt(22.0));
        _out->sk_FragColor = float4(sqrt(23.0));
        _out->sk_FragColor = float4(sqrt(24.0));
        _out->sk_FragColor += float4(1.0);
        _out->sk_FragColor -= float4(1.0);
        _out->sk_FragColor *= float4(2.0);
        _out->sk_FragColor /= float4(2.0);
    }

    {
        int4 _0_result;
        _0_result.x = 2;
        _0_result = int4(6, 7, 9, 11);
        _0_result = int4(7, 9, 9, 9);
        _0_result = int4(2, 4, 6, 8);
        _0_result = int4(12, 6, 4, 3);
        _out->sk_FragColor.x = 6.0;
        _out->sk_FragColor.x = 1.0;
        _out->sk_FragColor.x = -2.0;
        _out->sk_FragColor.x = 3.0;
        _out->sk_FragColor.x = 4.0;
        _out->sk_FragColor.x = -5.0;
        _out->sk_FragColor.x = 6.0;
        _out->sk_FragColor.x = 7.0;
        _out->sk_FragColor.x = -8.0;
        _out->sk_FragColor.x = 9.0;
        _out->sk_FragColor.x = -10.0;
        _0_result = int4(int(sqrt(1.0)));
        _0_result = int4(int(sqrt(2.0)));
        _0_result = int4(0);
        _0_result = int4(0);
        _0_result = int4(0);
        _0_result = int4(int(sqrt(6.0)));
        _0_result = int4(int(sqrt(7.0)));
        _0_result = int4(int(sqrt(8.0)));
        _0_result = int4(int(sqrt(9.0)));
        _0_result = int4(0);
        _0_result = int4(0);
        _0_result = int4(int(sqrt(12.0)));
        _0_result = int4(int(sqrt(13.0)));
        _0_result = int4(0);
        _0_result = int4(0);
        _0_result = int4(int(sqrt(16.0)));
        _0_result = int4(int(sqrt(17.0)));
        _0_result = int4(0);
        _0_result = int4(int(sqrt(19.0)));
        _0_result = int4(int(sqrt(19.5)));
        _0_result = int4(int(sqrt(20.0)));
        _0_result = int4(int(sqrt(21.0)));
        _0_result = int4(int(sqrt(22.0)));
        _0_result = int4(int(sqrt(23.0)));
        _0_result = int4(int(sqrt(24.0)));
        _0_result += int4(1);
        _0_result -= int4(1);
        _0_result *= int4(2);
        _0_result /= int4(2);
        _out->sk_FragColor = float4(_0_result);
    }

    return *_out;
}
