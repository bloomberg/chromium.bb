fn select_e3e028() {
  var res : vec4<bool> = select(vec4<bool>(), vec4<bool>(), vec4<bool>());
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
