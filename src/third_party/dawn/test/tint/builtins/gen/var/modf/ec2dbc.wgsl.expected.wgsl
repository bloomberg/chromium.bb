fn modf_ec2dbc() {
  var arg_0 = vec4<f32>();
  var res = modf(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  modf_ec2dbc();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  modf_ec2dbc();
}

@compute @workgroup_size(1)
fn compute_main() {
  modf_ec2dbc();
}
