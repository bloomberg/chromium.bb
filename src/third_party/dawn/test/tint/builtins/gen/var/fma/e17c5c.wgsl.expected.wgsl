fn fma_e17c5c() {
  var arg_0 = vec3<f32>();
  var arg_1 = vec3<f32>();
  var arg_2 = vec3<f32>();
  var res : vec3<f32> = fma(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  fma_e17c5c();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  fma_e17c5c();
}

@compute @workgroup_size(1)
fn compute_main() {
  fma_e17c5c();
}
