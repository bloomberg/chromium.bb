fn firstLeadingBit_a622c2() {
  var arg_0 = vec2<i32>();
  var res : vec2<i32> = firstLeadingBit(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  firstLeadingBit_a622c2();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  firstLeadingBit_a622c2();
}

@compute @workgroup_size(1)
fn compute_main() {
  firstLeadingBit_a622c2();
}
