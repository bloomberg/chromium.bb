void select_1e960b() {
  uint2 arg_0 = (0u).xx;
  uint2 arg_1 = (0u).xx;
  bool2 arg_2 = (false).xx;
  uint2 res = (arg_2 ? arg_1 : arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  select_1e960b();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  select_1e960b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  select_1e960b();
  return;
}
