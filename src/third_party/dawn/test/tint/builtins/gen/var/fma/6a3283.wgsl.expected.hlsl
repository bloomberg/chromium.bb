void fma_6a3283() {
  float4 arg_0 = (0.0f).xxxx;
  float4 arg_1 = (0.0f).xxxx;
  float4 arg_2 = (0.0f).xxxx;
  float4 res = mad(arg_0, arg_1, arg_2);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  fma_6a3283();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  fma_6a3283();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  fma_6a3283();
  return;
}
