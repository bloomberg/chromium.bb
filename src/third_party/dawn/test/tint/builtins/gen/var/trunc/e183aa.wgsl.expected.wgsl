fn trunc_e183aa() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = trunc(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  trunc_e183aa();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  trunc_e183aa();
}

@compute @workgroup_size(1)
fn compute_main() {
  trunc_e183aa();
}
