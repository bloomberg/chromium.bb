layout(key) float zero = floor(0.5);
layout(key) float one = ceil(0.5);

half4 main() {
    half4x4 m = half4x4(one);
    half4 n = half4(zero);
    bool4 b = bool4(true);

    n.x = abs(n.x);
    b.z = all(b.xy);
    b.w = any(b.xyz);
    n.xy = atan(n.xy);
    n.zwx = atan(n.yyy, n.zzz);
    n.xyzw = ceil(n.xyzw);
    n.x = clamp(n.y, n.z, n.w);
    n.y = cos(n.y);
    n.w = cross(n.xy, n.zw);
    n.xyz = degrees(n.xyz);
    n.w = distance(n.xz, n.yw);
    n.x = dot(n.yzw, n.yzw);
    b.xyz = equal(b.xxx, b.www);
    n.yz = exp(n.wx);
    n.zw = exp2(n.xy);
    n.x = faceforward(n.y, n.z, n.w);
    n = floor(n);
    n.yzw = fract(n.yzw);
    b.xy = greaterThan(n.xy, n.zw);
    b.xy = greaterThanEqual(n.xy, n.zw);
    n = inversesqrt(n);
    m = inverse(m);
    n.w = length(n.zyyx);
    b.xy = lessThan(n.xy, n.zw);
    b.xy = lessThanEqual(n.xy, n.zw);
    n.x = log(n.x);
    n.y = max(n.z, n.w);
    n.z = min(n.x, n.y);
    n.w = mod(n.y, n.z);
    n = normalize(n);
    b = not(b);
    n.x = pow(n.y, n.z);
    n.xyz = radians(n.yzw);
    n.xy = reflect(n.xy, n.zw);
    n.wz = refract(n.xy, n.zw, 2);
    n = saturate(n);
    n.x = sign(n.x);
    n.y = sin(n.y);
    n.zw = smoothstep(n.xx, n.yy, n.zz);
    n = sqrt(n);
    n.xy = step(n.xy, n.zw);
    n.x = tan(n.x);
    n = unpremul(n.aaaa);

    return half4(0, 1, 0, 1);
}
