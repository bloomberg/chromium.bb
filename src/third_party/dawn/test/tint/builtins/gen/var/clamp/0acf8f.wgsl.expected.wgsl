fn clamp_0acf8f() {
  var arg_0 = vec2<f32>();
  var arg_1 = vec2<f32>();
  var arg_2 = vec2<f32>();
  var res : vec2<f32> = clamp(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  clamp_0acf8f();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  clamp_0acf8f();
}

@compute @workgroup_size(1)
fn compute_main() {
  clamp_0acf8f();
}
