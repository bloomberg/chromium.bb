void clamp_7706d7() {
  uint2 arg_0 = (0u).xx;
  uint2 arg_1 = (0u).xx;
  uint2 arg_2 = (0u).xx;
  uint2 res = clamp(arg_0, arg_1, arg_2);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  clamp_7706d7();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  clamp_7706d7();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  clamp_7706d7();
  return;
}
