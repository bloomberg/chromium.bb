fn exp2_a9d0a7() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = exp2(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  exp2_a9d0a7();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  exp2_a9d0a7();
}

@compute @workgroup_size(1)
fn compute_main() {
  exp2_a9d0a7();
}
