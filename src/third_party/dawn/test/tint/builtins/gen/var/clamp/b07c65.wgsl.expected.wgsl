fn clamp_b07c65() {
  var arg_0 = 1;
  var arg_1 = 1;
  var arg_2 = 1;
  var res : i32 = clamp(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  clamp_b07c65();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  clamp_b07c65();
}

@compute @workgroup_size(1)
fn compute_main() {
  clamp_b07c65();
}
