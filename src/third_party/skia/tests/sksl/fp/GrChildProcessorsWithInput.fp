uniform half4 color;

in fragmentProcessor child1;
in fragmentProcessor child2;

void main() {
    half4 childIn = color;
    half4 childOut1 = sample(child1, childIn);
    half4 childOut2 = sample(child2, childOut1);
    sk_OutColor = childOut2;
}
