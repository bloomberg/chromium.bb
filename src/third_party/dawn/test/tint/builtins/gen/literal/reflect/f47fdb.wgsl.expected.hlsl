void reflect_f47fdb() {
  float3 res = reflect((0.0f).xxx, (0.0f).xxx);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  reflect_f47fdb();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  reflect_f47fdb();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  reflect_f47fdb();
  return;
}
