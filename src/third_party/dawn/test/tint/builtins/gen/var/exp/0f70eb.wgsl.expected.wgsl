fn exp_0f70eb() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = exp(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  exp_0f70eb();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  exp_0f70eb();
}

@compute @workgroup_size(1)
fn compute_main() {
  exp_0f70eb();
}
