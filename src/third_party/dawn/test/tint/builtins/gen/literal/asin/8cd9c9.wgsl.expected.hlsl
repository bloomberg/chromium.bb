void asin_8cd9c9() {
  float3 res = asin((0.0f).xxx);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  asin_8cd9c9();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  asin_8cd9c9();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  asin_8cd9c9();
  return;
}
