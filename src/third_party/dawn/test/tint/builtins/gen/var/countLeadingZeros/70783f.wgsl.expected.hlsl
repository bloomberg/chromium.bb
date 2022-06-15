uint2 tint_count_leading_zeros(uint2 v) {
  uint2 x = uint2(v);
  const uint2 b16 = ((x <= (65535u).xx) ? (16u).xx : (0u).xx);
  x = (x << b16);
  const uint2 b8 = ((x <= (16777215u).xx) ? (8u).xx : (0u).xx);
  x = (x << b8);
  const uint2 b4 = ((x <= (268435455u).xx) ? (4u).xx : (0u).xx);
  x = (x << b4);
  const uint2 b2 = ((x <= (1073741823u).xx) ? (2u).xx : (0u).xx);
  x = (x << b2);
  const uint2 b1 = ((x <= (2147483647u).xx) ? (1u).xx : (0u).xx);
  const uint2 is_zero = ((x == (0u).xx) ? (1u).xx : (0u).xx);
  return uint2((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_70783f() {
  uint2 arg_0 = (0u).xx;
  uint2 res = tint_count_leading_zeros(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  countLeadingZeros_70783f();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  countLeadingZeros_70783f();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  countLeadingZeros_70783f();
  return;
}
