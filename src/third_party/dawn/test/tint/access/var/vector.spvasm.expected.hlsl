void main_1() {
  float3 v = (0.0f).xxx;
  const float x_14 = v.y;
  const float3 x_16 = v;
  const float2 x_17 = float2(x_16.x, x_16.z);
  const float3 x_18 = v;
  const float3 x_19 = float3(x_18.x, x_18.z, x_18.y);
  return;
}

[numthreads(1, 1, 1)]
void main() {
  main_1();
  return;
}
