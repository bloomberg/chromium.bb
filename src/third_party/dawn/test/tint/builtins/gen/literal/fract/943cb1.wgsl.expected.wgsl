fn fract_943cb1() {
  var res : vec2<f32> = fract(vec2<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  fract_943cb1();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  fract_943cb1();
}

@compute @workgroup_size(1)
fn compute_main() {
  fract_943cb1();
}
