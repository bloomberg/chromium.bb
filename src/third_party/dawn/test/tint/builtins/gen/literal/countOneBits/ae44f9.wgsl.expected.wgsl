fn countOneBits_ae44f9() {
  var res : u32 = countOneBits(1u);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  countOneBits_ae44f9();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  countOneBits_ae44f9();
}

@compute @workgroup_size(1)
fn compute_main() {
  countOneBits_ae44f9();
}
