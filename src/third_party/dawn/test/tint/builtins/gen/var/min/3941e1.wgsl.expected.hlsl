void min_3941e1() {
  int4 arg_0 = (0).xxxx;
  int4 arg_1 = (0).xxxx;
  int4 res = min(arg_0, arg_1);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  min_3941e1();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  min_3941e1();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  min_3941e1();
  return;
}
