fn firstTrailingBit_50c072() {
  var arg_0 = vec2<i32>();
  var res : vec2<i32> = firstTrailingBit(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  firstTrailingBit_50c072();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  firstTrailingBit_50c072();
}

@compute @workgroup_size(1)
fn compute_main() {
  firstTrailingBit_50c072();
}
