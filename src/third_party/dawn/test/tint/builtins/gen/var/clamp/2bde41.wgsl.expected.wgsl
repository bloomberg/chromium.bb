fn clamp_2bde41() {
  var arg_0 = vec4<f32>();
  var arg_1 = vec4<f32>();
  var arg_2 = vec4<f32>();
  var res : vec4<f32> = clamp(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  clamp_2bde41();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  clamp_2bde41();
}

@compute @workgroup_size(1)
fn compute_main() {
  clamp_2bde41();
}
