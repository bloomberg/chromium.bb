fn select_fb7e53() {
  var arg_0 = vec2<bool>();
  var arg_1 = vec2<bool>();
  var arg_2 = bool();
  var res : vec2<bool> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_fb7e53();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_fb7e53();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_fb7e53();
}
