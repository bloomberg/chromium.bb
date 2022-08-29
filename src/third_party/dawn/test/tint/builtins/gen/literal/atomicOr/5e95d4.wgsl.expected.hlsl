RWByteAddressBuffer sb_rw : register(u0, space0);

uint tint_atomicOr(RWByteAddressBuffer buffer, uint offset, uint value) {
  uint original_value = 0;
  buffer.InterlockedOr(offset, value, original_value);
  return original_value;
}


void atomicOr_5e95d4() {
  uint res = tint_atomicOr(sb_rw, 0u, 1u);
}

void fragment_main() {
  atomicOr_5e95d4();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atomicOr_5e95d4();
  return;
}
