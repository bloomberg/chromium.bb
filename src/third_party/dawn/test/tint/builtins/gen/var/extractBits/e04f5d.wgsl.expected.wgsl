fn extractBits_e04f5d() {
  var arg_0 = vec3<i32>();
  var arg_1 = 1u;
  var arg_2 = 1u;
  var res : vec3<i32> = extractBits(arg_0, arg_1, arg_2);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  extractBits_e04f5d();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  extractBits_e04f5d();
}

@compute @workgroup_size(1)
fn compute_main() {
  extractBits_e04f5d();
}
