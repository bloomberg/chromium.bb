fn select_00b848() {
  var arg_0 = vec2<i32>();
  var arg_1 = vec2<i32>();
  var arg_2 = vec2<bool>();
  var res : vec2<i32> = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_00b848();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_00b848();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_00b848();
}
