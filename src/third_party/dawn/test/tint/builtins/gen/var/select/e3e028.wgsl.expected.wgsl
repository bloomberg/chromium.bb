fn select_e3e028() {
  var arg_0 = vec4<bool>();
  var arg_1 = vec4<bool>();
  var arg_2 = vec4<bool>();
  var res : vec4<bool> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_e3e028();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_e3e028();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_e3e028();
}
