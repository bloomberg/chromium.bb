fn max_44a39d() {
  var arg_0 = 1.0;
  var arg_1 = 1.0;
  var res : f32 = max(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  max_44a39d();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  max_44a39d();
}

@compute @workgroup_size(1)
fn compute_main() {
  max_44a39d();
}
