fn all_986c7b() {
  var res : bool = all(vec4<bool>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  all_986c7b();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  all_986c7b();
}

@compute @workgroup_size(1)
fn compute_main() {
  all_986c7b();
}
