fn pow_4a46c9() {
  var arg_0 = vec3<f32>();
  var arg_1 = vec3<f32>();
  var res : vec3<f32> = pow(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  pow_4a46c9();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  pow_4a46c9();
}

@compute @workgroup_size(1)
fn compute_main() {
  pow_4a46c9();
}
