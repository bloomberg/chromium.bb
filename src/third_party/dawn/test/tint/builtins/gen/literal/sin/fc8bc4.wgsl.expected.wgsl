fn sin_fc8bc4() {
  var res : vec2<f32> = sin(vec2<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  sin_fc8bc4();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  sin_fc8bc4();
}

@compute @workgroup_size(1)
fn compute_main() {
  sin_fc8bc4();
}
