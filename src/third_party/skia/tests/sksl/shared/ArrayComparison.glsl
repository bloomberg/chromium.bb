
out vec4 sk_FragColor;
uniform vec4 colorGreen;
uniform vec4 colorRed;
struct S {
    int x;
    int y;
};
vec4 main() {
    float f1[4] = float[4](1.0, 2.0, 3.0, 4.0);
    float f2[4] = float[4](1.0, 2.0, 3.0, 4.0);
    float f3[4] = float[4](1.0, 2.0, 3.0, -4.0);
    S s1[3] = S[3](S(1, 2), S(3, 4), S(5, 6));
    S s2[3] = S[3](S(1, 2), S(0, 0), S(5, 6));
    S s3[3] = S[3](S(1, 2), S(3, 4), S(5, 6));
    return ((f1 == f2 && f1 != f3) && s1 != s2) && s3 == s1 ? colorGreen : colorRed;
}
