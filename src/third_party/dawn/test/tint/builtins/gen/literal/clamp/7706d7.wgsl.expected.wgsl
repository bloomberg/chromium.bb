fn clamp_7706d7() {
  var res : vec2<u32> = clamp(vec2<u32>(), vec2<u32>(), vec2<u32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  clamp_7706d7();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  clamp_7706d7();
}

@compute @workgroup_size(1)
fn compute_main() {
  clamp_7706d7();
}
