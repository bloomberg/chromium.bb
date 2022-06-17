uint tint_count_trailing_zeros(uint v) {
  uint x = uint(v);
  const uint b16 = (bool((x & 65535u)) ? 0u : 16u);
  x = (x >> b16);
  const uint b8 = (bool((x & 255u)) ? 0u : 8u);
  x = (x >> b8);
  const uint b4 = (bool((x & 15u)) ? 0u : 4u);
  x = (x >> b4);
  const uint b2 = (bool((x & 3u)) ? 0u : 2u);
  x = (x >> b2);
  const uint b1 = (bool((x & 1u)) ? 0u : 1u);
  const uint is_zero = ((x == 0u) ? 1u : 0u);
  return uint((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countTrailingZeros_21e394() {
  uint arg_0 = 1u;
  uint res = tint_count_trailing_zeros(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countTrailingZeros_21e394();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countTrailingZeros_21e394();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countTrailingZeros_21e394();
  return;
}
