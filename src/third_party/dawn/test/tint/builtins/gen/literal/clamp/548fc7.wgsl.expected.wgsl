fn clamp_548fc7() {
  var res : vec3<u32> = clamp(vec3<u32>(), vec3<u32>(), vec3<u32>());
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  clamp_548fc7();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  clamp_548fc7();
}

@compute @workgroup_size(1)
fn compute_main() {
  clamp_548fc7();
}
