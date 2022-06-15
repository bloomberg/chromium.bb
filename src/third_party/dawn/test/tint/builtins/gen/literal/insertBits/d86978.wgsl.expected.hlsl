int4 tint_insert_bits(int4 v, int4 n, uint offset, uint count) {
  const uint s = min(offset, 32u);
  const uint e = min(32u, (s + count));
  const uint mask = (((1u << s) - 1u) ^ ((1u << e) - 1u));
  return (((n << uint4((s).xxxx)) & int4((int(mask)).xxxx)) | (v & int4((int(~(mask))).xxxx)));
}

void insertBits_d86978() {
  int4 res = tint_insert_bits((0).xxxx, (0).xxxx, 1u, 1u);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  insertBits_d86978();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  insertBits_d86978();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  insertBits_d86978();
  return;
}
