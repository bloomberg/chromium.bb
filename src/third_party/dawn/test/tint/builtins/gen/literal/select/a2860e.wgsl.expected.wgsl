fn select_a2860e() {
  var res : vec4<i32> = select(vec4<i32>(), vec4<i32>(), vec4<bool>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  select_a2860e();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  select_a2860e();
}

@compute @workgroup_size(1)
fn compute_main() {
  select_a2860e();
}
