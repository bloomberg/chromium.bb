fn transpose_4dc9a1() {
  var arg_0 = mat2x3<f32>();
  var res : mat3x2<f32> = transpose(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  transpose_4dc9a1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  transpose_4dc9a1();
}

@compute @workgroup_size(1)
fn compute_main() {
  transpose_4dc9a1();
}
