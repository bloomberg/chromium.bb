fn pow_04a908() {
  var arg_0 = vec4<f32>();
  var arg_1 = vec4<f32>();
  var res : vec4<f32> = pow(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  pow_04a908();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  pow_04a908();
}

@compute @workgroup_size(1)
fn compute_main() {
  pow_04a908();
}
