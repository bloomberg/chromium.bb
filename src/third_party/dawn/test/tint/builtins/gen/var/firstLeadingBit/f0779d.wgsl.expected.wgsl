fn firstLeadingBit_f0779d() {
  var arg_0 = 1u;
  var res : u32 = firstLeadingBit(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  firstLeadingBit_f0779d();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  firstLeadingBit_f0779d();
}

@compute @workgroup_size(1)
fn compute_main() {
  firstLeadingBit_f0779d();
}
