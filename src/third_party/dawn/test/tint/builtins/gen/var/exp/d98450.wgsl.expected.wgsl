fn exp_d98450() {
  var arg_0 = vec3<f32>();
  var res : vec3<f32> = exp(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  exp_d98450();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  exp_d98450();
}

@compute @workgroup_size(1)
fn compute_main() {
  exp_d98450();
}
