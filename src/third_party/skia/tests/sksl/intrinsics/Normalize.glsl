
out vec4 sk_FragColor;
in float a;
in vec4 b;
void main() {
    sk_FragColor.x = normalize(a);
    sk_FragColor = normalize(b);
}
