uint4 tint_count_leading_zeros(uint4 v) {
  uint4 x = uint4(v);
  const uint4 b16 = ((x <= (65535u).xxxx) ? (16u).xxxx : (0u).xxxx);
  x = (x << b16);
  const uint4 b8 = ((x <= (16777215u).xxxx) ? (8u).xxxx : (0u).xxxx);
  x = (x << b8);
  const uint4 b4 = ((x <= (268435455u).xxxx) ? (4u).xxxx : (0u).xxxx);
  x = (x << b4);
  const uint4 b2 = ((x <= (1073741823u).xxxx) ? (2u).xxxx : (0u).xxxx);
  x = (x << b2);
  const uint4 b1 = ((x <= (2147483647u).xxxx) ? (1u).xxxx : (0u).xxxx);
  const uint4 is_zero = ((x == (0u).xxxx) ? (1u).xxxx : (0u).xxxx);
  return uint4((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_f70103() {
  uint4 res = tint_count_leading_zeros((0u).xxxx);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countLeadingZeros_f70103();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countLeadingZeros_f70103();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countLeadingZeros_f70103();
  return;
}
