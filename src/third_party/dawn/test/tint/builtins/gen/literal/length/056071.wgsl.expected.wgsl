fn length_056071() {
  var res : f32 = length(vec3<f32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  length_056071();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  length_056071();
}

@compute @workgroup_size(1)
fn compute_main() {
  length_056071();
}
