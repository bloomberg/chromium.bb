fn reflect_05357e() {
  var res : vec4<f32> = reflect(vec4<f32>(), vec4<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  reflect_05357e();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  reflect_05357e();
}

@compute @workgroup_size(1)
fn compute_main() {
  reflect_05357e();
}
