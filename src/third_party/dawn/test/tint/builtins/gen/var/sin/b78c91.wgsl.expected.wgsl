fn sin_b78c91() {
  var arg_0 = 1.0;
  var res : f32 = sin(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  sin_b78c91();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  sin_b78c91();
}

@compute @workgroup_size(1)
fn compute_main() {
  sin_b78c91();
}
