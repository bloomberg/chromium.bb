void main_1() {
  const float x_24 = float3x3(float3(1.0f, 2.0f, 3.0f), float3(4.0f, 5.0f, 6.0f), float3(7.0f, 8.0f, 9.0f))[1u].y;
  return;
}

[numthreads(1, 1, 1)]
void main() {
  main_1();
  return;
}
