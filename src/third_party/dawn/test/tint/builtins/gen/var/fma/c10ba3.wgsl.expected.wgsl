fn fma_c10ba3() {
  var arg_0 = 1.0;
  var arg_1 = 1.0;
  var arg_2 = 1.0;
  var res : f32 = fma(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  fma_c10ba3();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  fma_c10ba3();
}

@compute @workgroup_size(1)
fn compute_main() {
  fma_c10ba3();
}
