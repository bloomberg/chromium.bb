fn inverseSqrt_c22347() {
  var arg_0 = vec4<f32>();
  var res : vec4<f32> = inverseSqrt(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  inverseSqrt_c22347();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  inverseSqrt_c22347();
}

@compute @workgroup_size(1)
fn compute_main() {
  inverseSqrt_c22347();
}
