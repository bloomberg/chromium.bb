fn faceForward_5afbd5() {
  var arg_0 = vec3<f32>();
  var arg_1 = vec3<f32>();
  var arg_2 = vec3<f32>();
  var res : vec3<f32> = faceForward(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  faceForward_5afbd5();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  faceForward_5afbd5();
}

@compute @workgroup_size(1)
fn compute_main() {
  faceForward_5afbd5();
}
