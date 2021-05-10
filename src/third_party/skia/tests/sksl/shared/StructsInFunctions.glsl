
out vec4 sk_FragColor;
uniform vec4 colorRed;
uniform vec4 colorGreen;
struct S {
    float x;
    int y;
};
S returns_a_struct() {
    S s;
    s.x = 1.0;
    s.y = 2;
    return s;
}
float accepts_a_struct(S s) {
    return s.x + float(s.y);
}
void modifies_a_struct(inout S s) {
    s.x++;
    s.y++;
}
vec4 main() {
    S s = returns_a_struct();
    float x = accepts_a_struct(s);
    modifies_a_struct(s);
    bool valid = (x == 3.0 && s.x == 2.0) && s.y == 3;
    return valid ? colorGreen : colorRed;
}
