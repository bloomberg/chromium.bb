cbuffer cbuffer_data : register(b0, space0) {
  uint4 data[4];
};

float3x3 tint_symbol_3(uint4 buffer[4], uint offset) {
  const uint scalar_offset = ((offset + 0u)) / 4;
  const uint scalar_offset_1 = ((offset + 16u)) / 4;
  const uint scalar_offset_2 = ((offset + 32u)) / 4;
  return float3x3(asfloat(buffer[scalar_offset / 4].xyz), asfloat(buffer[scalar_offset_1 / 4].xyz), asfloat(buffer[scalar_offset_2 / 4].xyz));
}

void main() {
  const float3 x = mul(tint_symbol_3(data, 0u), asfloat(data[3].xyz));
  return;
}
