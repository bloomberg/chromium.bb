fn cosh_da92dd() {
  var arg_0 = 1.0;
  var res : f32 = cosh(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  cosh_da92dd();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  cosh_da92dd();
}

@compute @workgroup_size(1)
fn compute_main() {
  cosh_da92dd();
}
