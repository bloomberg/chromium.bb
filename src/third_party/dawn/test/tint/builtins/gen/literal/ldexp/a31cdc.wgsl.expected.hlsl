void ldexp_a31cdc() {
  float3 res = ldexp((0.0f).xxx, (0).xxx);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  ldexp_a31cdc();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  ldexp_a31cdc();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  ldexp_a31cdc();
  return;
}
