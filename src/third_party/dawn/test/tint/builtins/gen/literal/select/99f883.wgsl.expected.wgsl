fn select_99f883() {
  var res : u32 = select(1u, 1u, bool());
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
