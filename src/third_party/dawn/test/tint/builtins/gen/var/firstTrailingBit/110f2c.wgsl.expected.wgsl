fn firstTrailingBit_110f2c() {
  var arg_0 = vec4<u32>();
  var res : vec4<u32> = firstTrailingBit(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  firstTrailingBit_110f2c();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  firstTrailingBit_110f2c();
}

@compute @workgroup_size(1)
fn compute_main() {
  firstTrailingBit_110f2c();
}
