fn atan_331e6d() {
  var arg_0 = vec3<f32>();
  var res : vec3<f32> = atan(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  atan_331e6d();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  atan_331e6d();
}

@compute @workgroup_size(1)
fn compute_main() {
  atan_331e6d();
}
