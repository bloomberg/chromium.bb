fn sign_b8f634() {
  var res : vec4<f32> = sign(vec4<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  sign_b8f634();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  sign_b8f634();
}

@compute @workgroup_size(1)
fn compute_main() {
  sign_b8f634();
}
