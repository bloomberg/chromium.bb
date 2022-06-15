fn refract_cbc1d2() {
  var arg_0 = vec3<f32>();
  var arg_1 = vec3<f32>();
  var arg_2 = 1.0;
  var res : vec3<f32> = refract(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  refract_cbc1d2();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  refract_cbc1d2();
}

@compute @workgroup_size(1)
fn compute_main() {
  refract_cbc1d2();
}
