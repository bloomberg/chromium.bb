void countOneBits_690cfc() {
  uint3 arg_0 = (0u).xxx;
  uint3 res = countbits(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countOneBits_690cfc();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countOneBits_690cfc();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countOneBits_690cfc();
  return;
}
