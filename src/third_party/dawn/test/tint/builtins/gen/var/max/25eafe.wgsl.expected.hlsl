void max_25eafe() {
  int3 arg_0 = (0).xxx;
  int3 arg_1 = (0).xxx;
  int3 res = max(arg_0, arg_1);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  max_25eafe();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  max_25eafe();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  max_25eafe();
  return;
}
