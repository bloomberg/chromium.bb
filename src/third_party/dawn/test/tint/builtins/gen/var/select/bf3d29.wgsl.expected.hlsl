void select_bf3d29() {
  float2 arg_0 = (0.0f).xx;
  float2 arg_1 = (0.0f).xx;
  bool arg_2 = false;
  float2 res = (arg_2 ? arg_1 : arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  select_bf3d29();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  select_bf3d29();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  select_bf3d29();
  return;
}
