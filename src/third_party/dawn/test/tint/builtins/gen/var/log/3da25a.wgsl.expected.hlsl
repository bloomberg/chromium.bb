void log_3da25a() {
  float4 arg_0 = (0.0f).xxxx;
  float4 res = log(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  log_3da25a();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  log_3da25a();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  log_3da25a();
  return;
}
