
out vec4 sk_FragColor;
uniform vec4 colorGreen;
uniform vec4 colorRed;
bool check(vec2 v1, vec2 v2, vec2 v3, vec3 v4, ivec2 v5, ivec2 v6, vec2 v7, vec2 v8, vec4 v9, ivec2 v10, bvec4 v11, vec2 v12, vec2 v13, vec2 v14, bvec2 v15, bvec2 v16, bvec3 v17) {
    return (((((((((((((((v1.x + v2.x) + v3.x) + v4.x) + float(v5.x)) + float(v6.x)) + v7.x) + v8.x) + v9.x) + float(v10.x)) + float(v11.x)) + v12.x) + v13.x) + v14.x) + float(v15.x)) + float(v16.x)) + float(v17.x) == 17.0;
}
vec4 main() {
    vec4 v9 = vec4(1.0, sqrt(2.0), vec2(ivec2(3, 4)));
    return check(vec2(1.0), vec2(1.0, 2.0), vec2(1.0), vec3(vec2(1.0), 1.0), ivec2(1), ivec2(vec2(1.0, 2.0)), vec2(ivec2(1, 2)), vec2(ivec2(1)), v9, ivec2(3, 1), bvec4(true, false, true, false), vec2(1.0, 0.0), vec2(0.0), vec2(bvec2(false)), bvec2(true), bvec2(vec2(1.0)), bvec3(true, bvec2(ivec2(77)))) ? colorGreen : colorRed;
}
