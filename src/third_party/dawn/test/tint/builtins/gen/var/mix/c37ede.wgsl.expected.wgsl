fn mix_c37ede() {
  var arg_0 = vec4<f32>();
  var arg_1 = vec4<f32>();
  var arg_2 = vec4<f32>();
  var res : vec4<f32> = mix(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  mix_c37ede();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  mix_c37ede();
}

@compute @workgroup_size(1)
fn compute_main() {
  mix_c37ede();
}
