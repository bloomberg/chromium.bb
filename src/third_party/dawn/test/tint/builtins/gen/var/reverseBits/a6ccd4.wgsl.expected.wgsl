fn reverseBits_a6ccd4() {
  var arg_0 = vec3<u32>();
  var res : vec3<u32> = reverseBits(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  reverseBits_a6ccd4();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  reverseBits_a6ccd4();
}

@compute @workgroup_size(1)
fn compute_main() {
  reverseBits_a6ccd4();
}
