fn transpose_31d679() {
  var arg_0 = mat2x2<f32>();
  var res : mat2x2<f32> = transpose(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  transpose_31d679();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  transpose_31d679();
}

@compute @workgroup_size(1)
fn compute_main() {
  transpose_31d679();
}
