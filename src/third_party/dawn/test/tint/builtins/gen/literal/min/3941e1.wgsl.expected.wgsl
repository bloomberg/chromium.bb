fn min_3941e1() {
  var res : vec4<i32> = min(vec4<i32>(), vec4<i32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  min_3941e1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  min_3941e1();
}

@compute @workgroup_size(1)
fn compute_main() {
  min_3941e1();
}
