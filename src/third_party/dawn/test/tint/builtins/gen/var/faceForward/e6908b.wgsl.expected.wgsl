fn faceForward_e6908b() {
  var arg_0 = vec2<f32>();
  var arg_1 = vec2<f32>();
  var arg_2 = vec2<f32>();
  var res : vec2<f32> = faceForward(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  faceForward_e6908b();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  faceForward_e6908b();
}

@compute @workgroup_size(1)
fn compute_main() {
  faceForward_e6908b();
}
