fn clamp_2bd567() {
  var arg_0 = 1.0;
  var arg_1 = 1.0;
  var arg_2 = 1.0;
  var res : f32 = clamp(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  clamp_2bd567();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  clamp_2bd567();
}

@compute @workgroup_size(1)
fn compute_main() {
  clamp_2bd567();
}
