fn log2_902988() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = log2(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  log2_902988();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  log2_902988();
}

@compute @workgroup_size(1)
fn compute_main() {
  log2_902988();
}
