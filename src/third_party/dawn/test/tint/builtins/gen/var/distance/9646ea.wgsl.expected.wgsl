fn distance_9646ea() {
  var arg_0 = vec4<f32>();
  var arg_1 = vec4<f32>();
  var res : f32 = distance(arg_0, arg_1);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  distance_9646ea();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  distance_9646ea();
}

@compute @workgroup_size(1)
fn compute_main() {
  distance_9646ea();
}
