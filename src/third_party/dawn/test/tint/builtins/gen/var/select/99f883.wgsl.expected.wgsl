fn select_99f883() {
  var arg_0 = 1u;
  var arg_1 = 1u;
  var arg_2 = bool();
  var res : u32 = select(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_99f883();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_99f883();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_99f883();
}
