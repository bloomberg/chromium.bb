fn clamp_7706d7() {
  var arg_0 = vec2<u32>();
  var arg_1 = vec2<u32>();
  var arg_2 = vec2<u32>();
  var res : vec2<u32> = clamp(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  clamp_7706d7();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  clamp_7706d7();
}

@compute @workgroup_size(1)
fn compute_main() {
  clamp_7706d7();
}
