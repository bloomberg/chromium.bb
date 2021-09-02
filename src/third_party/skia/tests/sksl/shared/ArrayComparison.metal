#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct S {
    int x;
    int y;
};
struct Uniforms {
    float4 colorGreen;
    float4 colorRed;
};
struct Inputs {
};
struct Outputs {
    float4 sk_FragColor [[color(0)]];
};

template <typename T, size_t N>
bool operator==(thread const array<T, N>& left, thread const array<T, N>& right) {
    for (size_t index = 0; index < N; ++index) {
        if (!(left[index] == right[index])) {
            return false;
        }
    }
    return true;
}

template <typename T, size_t N>
bool operator!=(thread const array<T, N>& left, thread const array<T, N>& right) {
    return !(left == right);
}
thread bool operator==(thread const S& left, thread const S& right) {
    return (left.x == right.x) &&
           (left.y == right.y);
}
thread bool operator!=(thread const S& left, thread const S& right) {
    return !(left == right);
}
fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    array<float, 4> f1 = array<float, 4>{1.0, 2.0, 3.0, 4.0};
    array<float, 4> f2 = array<float, 4>{1.0, 2.0, 3.0, 4.0};
    array<float, 4> f3 = array<float, 4>{1.0, 2.0, 3.0, -4.0};
    array<S, 3> s1 = array<S, 3>{S{1, 2}, S{3, 4}, S{5, 6}};
    array<S, 3> s2 = array<S, 3>{S{1, 2}, S{0, 0}, S{5, 6}};
    array<S, 3> s3 = array<S, 3>{S{1, 2}, S{3, 4}, S{5, 6}};
    _out.sk_FragColor = ((f1 == f2 && f1 != f3) && s1 != s2) && s3 == s1 ? _uniforms.colorGreen : _uniforms.colorRed;
    return _out;
}
