void reverseBits_7c4269() {
  int arg_0 = 1;
  int res = asint(reversebits(asuint(arg_0)));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  reverseBits_7c4269();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  reverseBits_7c4269();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  reverseBits_7c4269();
  return;
}
