void dpdxFine_f92fb6() {
  float3 res = ddx_fine((0.0f).xxx);
}

void fragment_main() {
  dpdxFine_f92fb6();
  return;
}
