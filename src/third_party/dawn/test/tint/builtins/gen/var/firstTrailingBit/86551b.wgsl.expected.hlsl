int4 tint_first_trailing_bit(int4 v) {
  uint4 x = uint4(v);
  const uint4 b16 = (bool4((x & (65535u).xxxx)) ? (0u).xxxx : (16u).xxxx);
  x = (x >> b16);
  const uint4 b8 = (bool4((x & (255u).xxxx)) ? (0u).xxxx : (8u).xxxx);
  x = (x >> b8);
  const uint4 b4 = (bool4((x & (15u).xxxx)) ? (0u).xxxx : (4u).xxxx);
  x = (x >> b4);
  const uint4 b2 = (bool4((x & (3u).xxxx)) ? (0u).xxxx : (2u).xxxx);
  x = (x >> b2);
  const uint4 b1 = (bool4((x & (1u).xxxx)) ? (0u).xxxx : (1u).xxxx);
  const uint4 is_zero = ((x == (0u).xxxx) ? (4294967295u).xxxx : (0u).xxxx);
  return int4((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

void firstTrailingBit_86551b() {
  int4 arg_0 = (0).xxxx;
  int4 res = tint_first_trailing_bit(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  firstTrailingBit_86551b();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  firstTrailingBit_86551b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  firstTrailingBit_86551b();
  return;
}
