fn tanh_5724b3() {
  var arg_0 = vec2<f32>();
  var res : vec2<f32> = tanh(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  tanh_5724b3();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  tanh_5724b3();
}

@compute @workgroup_size(1)
fn compute_main() {
  tanh_5724b3();
}
