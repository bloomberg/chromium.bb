void reverseBits_35fea9() {
  uint4 arg_0 = (0u).xxxx;
  uint4 res = reversebits(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  reverseBits_35fea9();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  reverseBits_35fea9();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  reverseBits_35fea9();
  return;
}
