Texture2DMS<float4> arg_0 : register(t0, space1);
static float4 tint_symbol_1 = (0.0f).xxxx;

void textureNumSamples_a3c8a0() {
  int res = 0;
  int3 tint_tmp;
  arg_0.GetDimensions(tint_tmp.x, tint_tmp.y, tint_tmp.z);
  const int x_16 = tint_tmp.z;
  res = x_16;
  return;
}

void tint_symbol_2(float4 tint_symbol) {
  tint_symbol_1 = tint_symbol;
  return;
}

void vertex_main_1() {
  textureNumSamples_a3c8a0();
  tint_symbol_2((0.0f).xxxx);
  return;
}

struct vertex_main_out {
  float4 tint_symbol_1_1;
};
struct tint_symbol_3 {
  float4 tint_symbol_1_1 : SV_Position;
};

vertex_main_out vertex_main_inner() {
  vertex_main_1();
  const vertex_main_out tint_symbol_4 = {tint_symbol_1};
  return tint_symbol_4;
}

tint_symbol_3 vertex_main() {
  const vertex_main_out inner_result = vertex_main_inner();
  tint_symbol_3 wrapper_result = (tint_symbol_3)0;
  wrapper_result.tint_symbol_1_1 = inner_result.tint_symbol_1_1;
  return wrapper_result;
}

void fragment_main_1() {
  textureNumSamples_a3c8a0();
  return;
}

void fragment_main() {
  fragment_main_1();
  return;
}

void compute_main_1() {
  textureNumSamples_a3c8a0();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  compute_main_1();
  return;
}
