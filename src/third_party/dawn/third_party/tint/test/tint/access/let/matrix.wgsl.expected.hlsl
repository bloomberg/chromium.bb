[numthreads(1, 1, 1)]
void main() {
  const float3x3 m = float3x3(float3(1.0f, 2.0f, 3.0f), float3(4.0f, 5.0f, 6.0f), float3(7.0f, 8.0f, 9.0f));
  const float3 v = m[1];
  const float f = v[1];
  return;
}
