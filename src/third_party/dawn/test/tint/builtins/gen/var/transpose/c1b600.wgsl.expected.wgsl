fn transpose_c1b600() {
  var arg_0 = mat4x4<f32>();
  var res : mat4x4<f32> = transpose(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  transpose_c1b600();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  transpose_c1b600();
}

@compute @workgroup_size(1)
fn compute_main() {
  transpose_c1b600();
}
