fn determinant_2b62ba() {
  var arg_0 = mat3x3<f32>();
  var res : f32 = determinant(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  determinant_2b62ba();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  determinant_2b62ba();
}

@compute @workgroup_size(1)
fn compute_main() {
  determinant_2b62ba();
}
