fn select_c41bd1() {
  var arg_0 = vec4<bool>();
  var arg_1 = vec4<bool>();
  var arg_2 = bool();
  var res : vec4<bool> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_c41bd1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_c41bd1();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_c41bd1();
}
