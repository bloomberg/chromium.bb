fn sinh_445e33() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = sinh(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  sinh_445e33();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  sinh_445e33();
}

@compute @workgroup_size(1)
fn compute_main() {
  sinh_445e33();
}
